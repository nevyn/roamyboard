/*
 * Cat: the status screen's cat runs over from the left half and lives on a playground with a
 * cat tree, a bowl and a bed while the app is open, focused or not. She roams, eats, naps and
 * plays as her needs (food, fun, energy) lead her; F fills the bowl, Space throws a ball, the
 * direction keys move a laser dot, Return pets her, and Escape sends her home, after which the
 * app closes. docs/pda.md describes her.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "cat_sprites.h"
#include "keys.h"
#include "shell.h"

/* The playground, in pixels from the app area's top left. */
#define PLAY_H (PDA_FB_H - PDA_TITLE_H)
#define MAX_X (PDA_FB_W - CAT_WIDTH)
#define BALL_SIZE 5
#define BED_X 3
#define EAT_X 33
#define WINDOW_X 40
#define ENTER_X 8
#define HOME_X (-CAT_WIDTH)

/* Speeds in pixels per second. */
#define WALK_SPEED 16
#define RUN_SPEED 64
#define DASH_SPEED 150
#define STALK_SPEED 8

#define FAST_TICK_MS 50
#define SLOW_TICK_MS 250
/** A longer gap between ticks (quick note, a busy queue) counts as this much. */
#define MAX_STEP_MS 250
/** She runs in this long after the app opens, once the left half's cat has dashed off. */
#define ENTER_DELAY_MS 500
#define JUMP_MS 550
#define POUNCE_MS 350
#define LEAP_MS 700
#define LASER_MS 6000
#define HEART_MS 1500
#define LASER_STEP_PX 4
/** Highest above her surface that she leaps at the laser dot. */
#define LEAP_REACH_PX 40

/** Needs change once per step: see live(). */
#define LIFE_STEP_MS 10000
/** Food that one portion in the bowl gives. */
#define PORTION 35

enum {
    USAGE_F = 0x09,
    USAGE_SPACE = 0x2C,
};

enum level { FLOOR, SHELF, TOP, LEVEL_COUNT };

/**
 * A surface that she stands on: its row, the columns that it spans, and the range of her
 * sprite's left edge on it.
 */
static const struct {
    int16_t surface, left, right, min_x, max_x;
} levels[LEVEL_COUNT] = {
    [FLOOR] = {134, 0, PDA_FB_W - 1, 0, MAX_X},
    [SHELF] = {94, 37, PDA_FB_W - 1, 27, MAX_X},
    [TOP] = {54, 0, 31, 0, 19},
};

/** Where she takes off and lands between level i and level i + 1. */
static const struct {
    int16_t lower_x, upper_x;
} hops[LEVEL_COUNT - 1] = {{30, 34}, {27, 12}};

enum doing {
    /** Off the left edge, before running in. */
    DOING_WAIT,
    DOING_WALK,
    DOING_JUMP,
    /** Stands for a moment, then decides; the chases pass through it. */
    DOING_STAND,
    DOING_SIT,
    DOING_EAT,
    /** At the empty bowl, asking for food. */
    DOING_BEG,
    DOING_SLEEP,
    DOING_LOAF,
    DOING_STRETCH,
    DOING_WATCH,
    DOING_PURR,
    DOING_POUNCE,
    DOING_LEAP,
    /** Eyes on the ball or the laser dot, before she pounces or leaps. */
    DOING_STARE,
};

enum goal {
    GOAL_NONE,
    GOAL_ROAM,
    GOAL_EAT,
    GOAL_BED,
    GOAL_WINDOW,
    GOAL_ZOOM,
    GOAL_BALL,
    GOAL_LASER,
    GOAL_HOME,
};

static int floor_div16(int32_t v) { return (int)(v >= 0 ? v / 16 : -((-v + 15) / 16)); }

static int x_px(const struct pda_cat_state *c) { return floor_div16(c->x16); }

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t between(struct pda_shell *shell, uint32_t lo, uint32_t hi) {
    return lo + pda_shell_random(shell, hi - lo + 1);
}

static void begin(struct pda_shell *shell, enum doing doing, uint32_t ms) {
    struct pda_cat_state *c = &shell->cat;
    c->doing = doing;
    c->since_ms = shell->now_ms;
    c->until_ms = shell->now_ms + ms;
}

static bool due(const struct pda_shell *shell) {
    return (int32_t)(shell->now_ms - shell->cat.until_ms) >= 0;
}

/** Adds to a need, up to full, and flags a save when its tens change. */
static void add_need(struct pda_shell *shell, uint8_t *need, int delta) {
    const int before = *need;
    *need = (uint8_t)clamp(before + delta, 0, PDA_CAT_FULL);
    if (before / 10 != *need / 10) {
        shell->cat_changed = true;
    }
}

static void walk_to(struct pda_shell *shell, int x) {
    struct pda_cat_state *c = &shell->cat;
    c->to_x = (int16_t)x;
    if (x != x_px(c)) {
        c->left = x < x_px(c);
    }
    begin(shell, DOING_WALK, 0);
}

static void arrive(struct pda_shell *shell);

/** Walks or jumps towards the goal's level and x, one leg at a time. */
static void next_leg(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (c->level == c->goal_level) {
        if (x_px(c) == c->goal_x) {
            arrive(shell);
        } else {
            walk_to(shell, c->goal_x);
        }
        return;
    }
    const bool up = c->goal_level > c->level;
    const int hop = up ? c->level : c->level - 1;
    const int from = up ? hops[hop].lower_x : hops[hop].upper_x;
    if (x_px(c) != from) {
        walk_to(shell, from);
        return;
    }
    c->from_x = (int16_t)from;
    c->to_x = up ? hops[hop].upper_x : hops[hop].lower_x;
    c->to_level = (uint8_t)(up ? c->level + 1 : c->level - 1);
    c->left = c->to_x < c->from_x;
    begin(shell, DOING_JUMP, JUMP_MS);
}

static void head_for(struct pda_shell *shell, enum goal goal, enum level level, int x, int speed) {
    struct pda_cat_state *c = &shell->cat;
    c->goal = goal;
    c->goal_level = level;
    c->goal_x = (int16_t)(goal == GOAL_HOME ? x : clamp(x, levels[level].min_x, levels[level].max_x));
    c->speed = (uint16_t)speed;
    next_leg(shell);
}

static void sit(struct pda_shell *shell, uint32_t ms) {
    shell->cat.goal = GOAL_NONE;
    begin(shell, DOING_SIT, ms);
}

static void arrive(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    switch ((enum goal)c->goal) {
    case GOAL_EAT:
        c->goal = GOAL_NONE;
        c->left = false;
        begin(shell, c->needs.bowl ? DOING_EAT : DOING_BEG, c->needs.bowl ? 0 : 8000);
        return;
    case GOAL_BED:
        c->goal = GOAL_NONE;
        c->left = false;
        begin(shell, DOING_SLEEP, 0);
        return;
    case GOAL_WINDOW:
        c->goal = GOAL_NONE;
        c->left = false;
        begin(shell, DOING_WATCH, between(shell, 4000, 10000));
        return;
    case GOAL_HOME:
        c->here = false;
        return;
    case GOAL_BALL:
    case GOAL_LASER:
        begin(shell, DOING_STAND, 0);
        return;
    case GOAL_ROAM:
    case GOAL_ZOOM:
    case GOAL_NONE:
        break;
    }
    c->goal = GOAL_NONE;
    begin(shell, DOING_STAND, between(shell, 800, 2500));
}

/** Picks what she does next, from her needs and a little chance. */
static void decide(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    const struct pda_cat_saved *n = &c->needs;
    if (n->energy < 25) {
        head_for(shell, GOAL_BED, TOP, BED_X, WALK_SPEED);
        return;
    }
    if (n->food < 35) {
        head_for(shell, GOAL_EAT, FLOOR, EAT_X, n->food < 15 ? RUN_SPEED : WALK_SPEED);
        return;
    }
    if (c->ball && n->fun < 70 && pda_shell_random(shell, 3) == 0) {
        c->kicks = 0;
        c->kick_limit = (uint8_t)between(shell, 2, 4);
        c->goal = GOAL_BALL;
        begin(shell, DOING_STAND, 0);
        return;
    }
    const uint32_t r = pda_shell_random(shell, 100);
    if (r < 35) {
        const enum level level = (enum level)pda_shell_random(shell, LEVEL_COUNT);
        head_for(shell, GOAL_ROAM, level,
                 (int)between(shell, (uint32_t)levels[level].min_x, (uint32_t)levels[level].max_x),
                 WALK_SPEED);
    } else if (r < 50) {
        sit(shell, between(shell, 3000, 8000));
    } else if (r < 62) {
        c->goal = GOAL_NONE;
        begin(shell, DOING_LOAF, between(shell, 10000, 25000));
    } else if (r < 72) {
        head_for(shell, GOAL_WINDOW, SHELF, WINDOW_X, WALK_SPEED);
    } else if (r < 80) {
        c->goal = GOAL_NONE;
        begin(shell, DOING_STRETCH, 900);
    } else if (r < 90 && n->energy > 50) {
        head_for(shell, GOAL_ZOOM, FLOOR, x_px(c) < MAX_X / 2 ? MAX_X : 0, RUN_SPEED);
    } else if (n->energy < 70) {
        c->goal = GOAL_NONE;
        begin(shell, DOING_SLEEP, 0);
    } else {
        sit(shell, between(shell, 2000, 5000));
    }
}

/** After a jump, pounce or leap: on towards the goal, or a seat. */
static void resume(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    switch ((enum goal)c->goal) {
    case GOAL_BALL:
    case GOAL_LASER:
        begin(shell, DOING_STAND, 0);
        break;
    case GOAL_NONE:
        sit(shell, 2000);
        break;
    default:
        next_leg(shell);
        break;
    }
}

/** She is in the middle of a move that ends on its own before she can heed anything new. */
static bool committed(const struct pda_cat_state *c) {
    return c->doing == DOING_WAIT || c->doing == DOING_JUMP || c->doing == DOING_POUNCE ||
           c->doing == DOING_LEAP;
}

/** Turns her to something new that the user started, unless she is mid-move or leaving. */
static bool heed(struct pda_shell *shell, enum goal goal) {
    struct pda_cat_state *c = &shell->cat;
    if (c->goal == GOAL_HOME) {
        return false;
    }
    c->goal = goal;
    if (!committed(c)) {
        begin(shell, DOING_STAND, 0);
    }
    return true;
}

/* The ball and the laser dot. */

static int ball_x(const struct pda_cat_state *c) { return floor_div16(c->ball_x16); }

static bool ball_moving(const struct pda_cat_state *c) {
    return c->ball && (c->ball_vx16 != 0 || c->ball_vh16 != 0 || c->ball_h16 != 0);
}

/** Bounces the ball along the floor: gravity, the walls and some friction. */
static void move_ball(struct pda_cat_state *c, uint32_t dt) {
    if (!ball_moving(c)) {
        return;
    }
    const int32_t gravity16 = 400 * 16;
    c->ball_vh16 -= gravity16 * (int32_t)dt / 1000;
    c->ball_h16 += c->ball_vh16 * (int32_t)dt / 1000;
    if (c->ball_h16 <= 0) {
        c->ball_h16 = 0;
        c->ball_vh16 = c->ball_vh16 < -40 * 16 ? -c->ball_vh16 * 45 / 100 : 0;
    }
    c->ball_x16 += c->ball_vx16 * (int32_t)dt / 1000;
    const int32_t max16 = (PDA_FB_W - BALL_SIZE) * 16;
    if (c->ball_x16 < 0) {
        c->ball_x16 = -c->ball_x16;
        c->ball_vx16 = -c->ball_vx16 * 3 / 4;
    } else if (c->ball_x16 > max16) {
        c->ball_x16 = 2 * max16 - c->ball_x16;
        c->ball_vx16 = -c->ball_vx16 * 3 / 4;
    }
    if (c->ball_h16 == 0) {
        c->ball_vx16 -= c->ball_vx16 * (int32_t)dt / 1200;
        if (c->ball_vx16 > -2 * 16 && c->ball_vx16 < 2 * 16) {
            c->ball_vx16 = 0;
        }
    }
}

/** The surface under the laser dot: the nearest at or below it. */
static enum level level_under(int x, int y) {
    enum level best = FLOOR;
    for (int i = SHELF; i < LEVEL_COUNT; i++) {
        if (x >= levels[i].left && x <= levels[i].right && levels[i].surface >= y - 2 &&
            levels[i].surface < levels[best].surface) {
            best = (enum level)i;
        }
    }
    return best;
}

/** Her paw's x: the front of her sprite. */
static int paw_x(const struct pda_cat_state *c) { return x_px(c) + (c->left ? 1 : CAT_WIDTH - 2); }

/** Where her sprite's left edge must be for her paw to reach x, coming from where she is. */
static int reach_x(const struct pda_cat_state *c, int x, bool *left) {
    *left = x < x_px(c) + CAT_WIDTH / 2;
    return *left ? x - 1 : x - CAT_WIDTH + 2;
}

/** Steers the walk under way, or starts one, towards x on the current level at speed. */
static void steer(struct pda_shell *shell, int x, int speed) {
    struct pda_cat_state *c = &shell->cat;
    c->speed = (uint16_t)speed;
    if (c->doing == DOING_WALK) {
        c->to_x = (int16_t)x;
        c->left = x < x_px(c);
    } else {
        walk_to(shell, x);
    }
}

/** Takes her to target_level, unless she is on it or on her way. */
static bool reach_level(struct pda_shell *shell, enum level level, int x) {
    struct pda_cat_state *c = &shell->cat;
    c->goal_x = (int16_t)clamp(x, levels[level].min_x, levels[level].max_x);
    if (c->level == level) {
        return true;
    }
    c->speed = RUN_SPEED;
    if (c->goal_level != level || c->doing != DOING_WALK) {
        c->goal_level = level;
        next_leg(shell);
    }
    return false;
}

static void chase_ball(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (!c->ball) {
        sit(shell, 1500);
        return;
    }
    bool left;
    const int target = reach_x(c, ball_x(c) + BALL_SIZE / 2, &left);
    if (!reach_level(shell, FLOOR, target)) {
        return;
    }
    const int x = clamp(target, levels[FLOOR].min_x, levels[FLOOR].max_x);
    if (x_px(c) != x) {
        steer(shell, x, RUN_SPEED);
        return;
    }
    c->left = left;
    if (c->ball_h16 < 3 * 16) {
        begin(shell, DOING_POUNCE, POUNCE_MS);
    } else if (c->doing != DOING_STARE) {
        begin(shell, DOING_STARE, 0);
    }
}

static void bat_ball(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    const int reach = paw_x(c) - (ball_x(c) + BALL_SIZE / 2);
    c->kicks++;
    if (c->ball && c->ball_h16 < 3 * 16 && reach >= -6 && reach <= 6) {
        const int32_t speed = (int32_t)between(shell, 50, 110);
        c->ball_vx16 = (c->left ? -speed : speed) * 16;
        c->ball_vh16 = 40 * 16;
        c->ball_h16 = 1;
        add_need(shell, &c->needs.fun, 6);
    }
    if (c->kicks >= c->kick_limit || c->needs.fun >= PDA_CAT_FULL) {
        sit(shell, 3000);
    } else {
        begin(shell, DOING_STAND, 0);
    }
}

static void chase_laser(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (!c->laser) {
        sit(shell, 1500);
        return;
    }
    const enum level level = level_under(c->laser_x, c->laser_y);
    const int above = levels[level].surface - c->laser_y;
    const bool on_surface = above <= 4;
    bool left;
    int target = on_surface ? reach_x(c, c->laser_x, &left) : c->laser_x - CAT_WIDTH / 2;
    if (!on_surface) {
        left = c->laser_x < x_px(c) + CAT_WIDTH / 2;
    }
    if (!reach_level(shell, level, target)) {
        return;
    }
    target = clamp(target, levels[level].min_x, levels[level].max_x);
    const int distance = target > x_px(c) ? target - x_px(c) : x_px(c) - target;
    if (distance > 0) {
        steer(shell, target, distance > 20 ? RUN_SPEED : on_surface ? STALK_SPEED : WALK_SPEED);
        return;
    }
    c->left = left;
    if (c->doing != DOING_STARE) {
        begin(shell, DOING_STARE, on_surface ? 600 : 500);
        return;
    }
    if (!due(shell)) {
        return;
    }
    if (on_surface) {
        begin(shell, DOING_POUNCE, POUNCE_MS);
    } else if (above <= LEAP_REACH_PX) {
        // Her sprite's top stays below the needs at the top of the playground.
        const int room = levels[level].surface - CAT_HEIGHT - 9;
        c->leap_px = (uint8_t)clamp(above - 6, 4, room);
        begin(shell, DOING_LEAP, LEAP_MS);
    }
}

/* Living. */

/** Changes her needs as time passes: hunger and boredom grow, sleep and rest restore energy. */
static void live(struct pda_shell *shell, uint32_t dt) {
    struct pda_cat_state *c = &shell->cat;
    c->life_ms += dt;
    while (c->life_ms >= LIFE_STEP_MS) {
        c->life_ms -= LIFE_STEP_MS;
        c->life_steps++;
        if (c->life_steps % 9 == 0) {
            add_need(shell, &c->needs.food, -1);
        }
        if (c->life_steps % 6 == 0) {
            add_need(shell, &c->needs.fun, -1);
        }
        if (c->doing == DOING_SLEEP) {
            add_need(shell, &c->needs.energy, 2);
        } else if (c->doing == DOING_LOAF) {
            add_need(shell, &c->needs.energy, 1);
        } else if (c->life_steps % 2 == 0) {
            add_need(shell, &c->needs.energy, -1);
        }
    }
}

/** One bite a second, until she is full or the bowl is empty. */
static void eat(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (shell->now_ms - c->since_ms >= 1000) {
        c->since_ms += 1000;
        add_need(shell, &c->needs.food, 4);
        c->eaten += 4;
        if (c->eaten >= PORTION) {
            c->eaten = 0;
            c->needs.bowl--;
            shell->cat_changed = true;
        }
    }
    if (c->needs.food >= PDA_CAT_FULL || c->needs.bowl == 0) {
        sit(shell, 3000);
    }
}

/** Advances what she does by dt. */
static void behave(struct pda_shell *shell, uint32_t dt) {
    struct pda_cat_state *c = &shell->cat;
    switch ((enum doing)c->doing) {
    case DOING_WAIT:
        if (due(shell)) {
            resume(shell);
        }
        break;
    case DOING_WALK: {
        const int32_t to16 = c->to_x * 16;
        int32_t step = (int32_t)c->speed * 16 * (int32_t)dt / 1000;
        step = step < 1 ? 1 : step;
        if (c->x16 < to16) {
            c->x16 = c->x16 + step >= to16 ? to16 : c->x16 + step;
        } else {
            c->x16 = c->x16 - step <= to16 ? to16 : c->x16 - step;
        }
        if (c->x16 == to16) {
            next_leg(shell);
        }
        break;
    }
    case DOING_JUMP:
        if (due(shell)) {
            c->level = c->to_level;
            c->x16 = c->to_x * 16;
            next_leg(shell);
        }
        break;
    case DOING_POUNCE:
        if (!due(shell)) {
            break;
        }
        if (c->goal == GOAL_BALL) {
            bat_ball(shell);
        } else if (c->goal == GOAL_LASER) {
            add_need(shell, &c->needs.fun, 3);
            begin(shell, DOING_STARE, 1200);
        } else {
            resume(shell);
        }
        break;
    case DOING_LEAP:
        if (due(shell)) {
            add_need(shell, &c->needs.fun, 3);
            if (c->goal == GOAL_LASER) {
                begin(shell, DOING_STARE, 900);
            } else {
                resume(shell);
            }
        }
        break;
    case DOING_EAT:
        eat(shell);
        break;
    case DOING_SLEEP:
        if (c->needs.energy >= PDA_CAT_FULL) {
            begin(shell, DOING_STRETCH, 900);
        }
        break;
    case DOING_STAND:
    case DOING_STARE:
        if (c->goal == GOAL_BALL || c->goal == GOAL_LASER) {
            break;
        }
        if (c->goal != GOAL_NONE && c->goal != GOAL_ROAM && c->goal != GOAL_ZOOM) {
            next_leg(shell);
        } else if (due(shell)) {
            decide(shell);
        }
        break;
    case DOING_SIT:
    case DOING_BEG:
    case DOING_LOAF:
    case DOING_STRETCH:
    case DOING_WATCH:
    case DOING_PURR:
        if (due(shell)) {
            decide(shell);
        }
        break;
    }
    if (c->goal == GOAL_BALL &&
        (c->doing == DOING_STAND || c->doing == DOING_STARE || c->doing == DOING_WALK)) {
        chase_ball(shell);
    } else if (c->goal == GOAL_LASER &&
               (c->doing == DOING_STAND || c->doing == DOING_STARE || c->doing == DOING_WALK)) {
        chase_laser(shell);
    }
}

/* What the screen shows. */

struct scene {
    /** CAT_FRAME_COUNT for none. */
    uint8_t frame;
    int16_t x, top;
    bool bubble;
    bool ball;
    int16_t ball_x, ball_y;
    bool laser;
    int16_t laser_x, laser_y;
    int16_t heart_x[PDA_CAT_HEARTS], heart_y[PDA_CAT_HEARTS];
    uint8_t meters[3];
    uint8_t bowl;
    /** 0 a plain window, 1 the sun, 2 the moon. */
    uint8_t sky;
};

#define NO_HEART INT16_MIN
#define METER_PX 11

/** Height from per-mille progress p: a quadratic ease (up decelerates, down accelerates) and a hop. */
static int arc_y(int from, int to, int p, int hop) {
    const int ease = to < from ? 2 * p - p * p / 1000 : p * p / 1000;
    return from + (to - from) * ease / 1000 - hop * 4 * p * (1000 - p) / 1000000;
}

static void pose(const struct pda_shell *shell, struct scene *s) {
    const struct pda_cat_state *c = &shell->cat;
    const uint32_t t = shell->now_ms - c->since_ms;
    const int x = x_px(c);
    int feet = levels[c->level].surface;
    enum cat_frame frame = CAT_WALK_1;
    s->x = (int16_t)x;
    switch ((enum doing)c->doing) {
    case DOING_WAIT:
        s->frame = CAT_FRAME_COUNT;
        return;
    case DOING_WALK:
        if (c->speed >= RUN_SPEED) {
            frame = (enum cat_frame)(CAT_RUN_0 + (x / 5 % 4 + 4) % 4);
        } else if (c->speed == STALK_SPEED) {
            frame = (x / 2) % 2 ? CAT_STALK_1 : CAT_STALK_0;
        } else {
            frame = (enum cat_frame)(CAT_WALK_0 + (x / 3 % 4 + 4) % 4);
        }
        break;
    case DOING_JUMP: {
        const int to_feet = levels[c->to_level].surface;
        if (t < 100) {
            frame = CAT_CROUCH;
            s->x = c->from_x;
        } else if (t >= JUMP_MS - 100) {
            frame = CAT_CROUCH;
            s->x = c->to_x;
            feet = to_feet;
        } else {
            const int p = (int)(t - 100) * 1000 / (JUMP_MS - 200);
            s->x = (int16_t)(c->from_x + (c->to_x - c->from_x) * p / 1000);
            feet = arc_y(feet, to_feet, p, 6);
            const int next = arc_y(levels[c->level].surface, to_feet, p + 50 > 1000 ? 1000 : p + 50, 6);
            frame = next < feet ? CAT_JUMP : CAT_FALL;
        }
        break;
    }
    case DOING_STAND:
        frame = CAT_WALK_1;
        break;
    case DOING_SIT: {
        const uint32_t phase = t % 4000;
        frame = t < 1200 ? CAT_WALK_1 : phase >= 3750 ? CAT_BLINK : CAT_SIT;
        break;
    }
    case DOING_EAT:
        frame = (t / 300) % 2 ? CAT_STALK_0 : CAT_HUNT;
        break;
    case DOING_BEG:
        frame = CAT_SIT_UP;
        s->bubble = (t / 1000) % 3 != 2;
        break;
    case DOING_SLEEP:
        frame = (shell->now_ms / 1000) % 2 ? CAT_SLEEP_1 : CAT_SLEEP_0;
        break;
    case DOING_LOAF:
        frame = CAT_LOAF;
        break;
    case DOING_STRETCH:
        frame = CAT_STRETCH;
        break;
    case DOING_WATCH:
        frame = CAT_SIT_UP;
        break;
    case DOING_PURR:
        frame = CAT_BLINK;
        break;
    case DOING_POUNCE:
        if (t < 100) {
            frame = CAT_READY_0;
        } else {
            const int p = (int)clamp((int)(t - 100) * 1000 / (POUNCE_MS - 100), 0, 1000);
            frame = CAT_POUNCE;
            feet -= 5 * 4 * p * (1000 - p) / 1000000;
        }
        break;
    case DOING_LEAP:
        if (t < 200 || t >= 650) {
            frame = CAT_CROUCH;
        } else {
            const int p = (int)(t - 200) * 1000 / 450;
            feet -= c->leap_px * 4 * p * (1000 - p) / 1000000;
            frame = p < 500 ? CAT_JUMP : CAT_FALL;
        }
        break;
    case DOING_STARE:
        if (c->goal == GOAL_LASER &&
            levels[level_under(c->laser_x, c->laser_y)].surface - c->laser_y <= 4) {
            frame = (t / 150) % 2 ? CAT_READY_1 : CAT_READY_0;
        } else {
            frame = CAT_HUNT_UP;
        }
        break;
    }
    s->frame = (uint8_t)(c->left ? CAT_FACING_LEFT + frame : frame);
    s->top = (int16_t)(feet - CAT_HEIGHT);
}

static void scene_of(const struct pda_shell *shell, struct scene *s) {
    const struct pda_cat_state *c = &shell->cat;
    memset(s, 0, sizeof(*s));
    pose(shell, s);
    if (c->ball) {
        s->ball = true;
        s->ball_x = (int16_t)ball_x(c);
        s->ball_y = (int16_t)(levels[FLOOR].surface - BALL_SIZE - floor_div16(c->ball_h16));
    }
    if (c->laser) {
        s->laser = true;
        s->laser_x = c->laser_x;
        s->laser_y = c->laser_y;
    }
    for (int i = 0; i < PDA_CAT_HEARTS; i++) {
        const uint32_t age = shell->now_ms - c->heart_ms[i];
        const bool shown = c->heart_ms[i] != 0 && age < HEART_MS;
        s->heart_x[i] = shown ? c->heart_x[i] : NO_HEART;
        s->heart_y[i] = shown ? (int16_t)(c->heart_y[i] - (int)age * 20 / HEART_MS) : NO_HEART;
    }
    s->meters[0] = (uint8_t)(c->needs.food * METER_PX / PDA_CAT_FULL);
    s->meters[1] = (uint8_t)(c->needs.fun * METER_PX / PDA_CAT_FULL);
    s->meters[2] = (uint8_t)(c->needs.energy * METER_PX / PDA_CAT_FULL);
    s->bowl = c->needs.bowl;
    struct pda_date_time now;
    if (pda_shell_time(shell, &now)) {
        s->sky = now.hour >= 7 && now.hour < 19 ? 1 : 2;
    }
}

static uint32_t hash_scene(const struct scene *s) {
    uint32_t h = 2166136261u;
    const uint8_t *bytes = (const uint8_t *)s;
    for (size_t i = 0; i < sizeof(*s); i++) {
        h = (h ^ bytes[i]) * 16777619u;
    }
    return h;
}

/* Ticks and keys. */

static bool hearts_shown(const struct pda_shell *shell) {
    for (int i = 0; i < PDA_CAT_HEARTS; i++) {
        if (shell->cat.heart_ms[i] != 0 && shell->now_ms - shell->cat.heart_ms[i] < HEART_MS) {
            return true;
        }
    }
    return false;
}

/** Fast ticks while anything moves; slow ones for blinks, snores and decisions. */
static void pace(struct pda_shell *shell) {
    const struct pda_cat_state *c = &shell->cat;
    bool calm = false;
    switch ((enum doing)c->doing) {
    case DOING_SIT:
    case DOING_BEG:
    case DOING_SLEEP:
    case DOING_LOAF:
    case DOING_STRETCH:
    case DOING_WATCH:
    case DOING_PURR:
        calm = true;
        break;
    default:
        break;
    }
    if (ball_moving(c) || hearts_shown(shell)) {
        calm = false;
    }
    shell->tick_ms = calm ? SLOW_TICK_MS : FAST_TICK_MS;
}

static enum pda_app_result redraw_if_changed(struct pda_shell *shell) {
    struct scene s;
    scene_of(shell, &s);
    const uint32_t h = hash_scene(&s);
    if (h == shell->cat.drawn) {
        return PDA_APP_NONE;
    }
    shell->cat.drawn = h;
    return PDA_APP_REDRAW;
}

static enum pda_app_result cat_tick(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    uint32_t dt = shell->now_ms - c->last_ms;
    c->last_ms = shell->now_ms;
    dt = dt > MAX_STEP_MS ? MAX_STEP_MS : dt;
    if (c->doing != DOING_WAIT) {
        live(shell, dt);
    }
    move_ball(c, dt);
    if (c->laser && shell->now_ms - c->laser_ms >= LASER_MS) {
        c->laser = false;
    }
    behave(shell, dt);
    if (!c->here) {
        return PDA_APP_CLOSE;
    }
    pace(shell);
    return redraw_if_changed(shell);
}

static void cat_open(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (!c->here) {
        c->here = true;
        c->x16 = HOME_X * 16;
        c->level = FLOOR;
        c->left = false;
        c->ball = false;
        c->laser = false;
        memset(c->heart_ms, 0, sizeof(c->heart_ms));
        c->goal = GOAL_ROAM;
        c->goal_level = FLOOR;
        c->goal_x = ENTER_X;
        c->speed = DASH_SPEED;
        begin(shell, DOING_WAIT, ENTER_DELAY_MS);
    }
    c->last_ms = shell->now_ms;
    c->drawn = 0;
    shell->tick_ms = FAST_TICK_MS;
}

static void pet(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (c->doing == DOING_WAIT) {
        return;
    }
    const int i = c->next_heart++ % PDA_CAT_HEARTS;
    c->heart_ms[i] = shell->now_ms ? shell->now_ms : 1;
    c->heart_x[i] = (int16_t)(x_px(c) + (c->left ? 1 : CAT_WIDTH - 8) +
                              (int)pda_shell_random(shell, 5) - 2);
    c->heart_y[i] = (int16_t)(levels[c->level].surface - CAT_HEIGHT - 6);
    add_need(shell, &c->needs.fun, 5);
    if (c->doing != DOING_SLEEP && !committed(c) && c->goal != GOAL_HOME) {
        sit(shell, 0);
        begin(shell, DOING_PURR, 2500);
    }
}

static void throw_ball(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    c->ball = true;
    c->ball_x16 = (PDA_FB_W - BALL_SIZE - 2) * 16;
    c->ball_h16 = 70 * 16;
    c->ball_vx16 = -(int32_t)between(shell, 30, 80) * 16;
    c->ball_vh16 = 20 * 16;
    if (c->doing == DOING_SLEEP && c->needs.energy < 40) {
        return;
    }
    c->kicks = 0;
    c->kick_limit = (uint8_t)between(shell, 3, 6);
    heed(shell, GOAL_BALL);
}

static void move_laser(struct pda_shell *shell, enum pda_dir dir) {
    struct pda_cat_state *c = &shell->cat;
    if (!c->laser) {
        c->laser = true;
        c->laser_x = PDA_FB_W / 2;
        c->laser_y = levels[FLOOR].surface - 2;
    } else {
        const int dx = dir == PDA_DIR_LEFT ? -1 : dir == PDA_DIR_RIGHT ? 1 : 0;
        const int dy = dir == PDA_DIR_UP ? -1 : dir == PDA_DIR_DOWN ? 1 : 0;
        c->laser_x = (int16_t)clamp(c->laser_x + dx * LASER_STEP_PX, 1, PDA_FB_W - 2);
        c->laser_y = (int16_t)clamp(c->laser_y + dy * LASER_STEP_PX, 12, PLAY_H - 2);
    }
    c->laser_ms = shell->now_ms;
    if (c->doing == DOING_SLEEP && c->needs.energy < 30) {
        return;
    }
    heed(shell, GOAL_LASER);
}

static void feed(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (c->needs.bowl < PDA_CAT_BOWL_MAX) {
        c->needs.bowl = PDA_CAT_BOWL_MAX;
        shell->cat_changed = true;
    }
    if (c->needs.food < 80 && c->doing != DOING_SLEEP && c->doing != DOING_EAT &&
        c->goal != GOAL_BALL && c->goal != GOAL_LASER && heed(shell, GOAL_EAT)) {
        c->goal_level = FLOOR;
        c->goal_x = EAT_X;
        c->speed = RUN_SPEED;
    }
}

static void go_home(struct pda_shell *shell) {
    struct pda_cat_state *c = &shell->cat;
    if (c->doing == DOING_WAIT || c->goal == GOAL_HOME) {
        // Impatient: a second Escape, or one before she has even arrived, ends it at once.
        c->here = false;
        return;
    }
    c->goal = GOAL_HOME;
    c->goal_level = FLOOR;
    c->goal_x = HOME_X;
    c->speed = DASH_SPEED;
    c->laser = false;
    if (!committed(c)) {
        next_leg(shell);
    }
}

static enum pda_app_result cat_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                   bool pressed) {
    (void)implicit_mods;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    const enum pda_dir dir = pda_key_dir(usage);
    if (dir != PDA_DIR_NONE) {
        move_laser(shell, dir);
    } else if (usage == PDA_USAGE_ESCAPE) {
        go_home(shell);
        if (!shell->cat.here) {
            return PDA_APP_CLOSE;
        }
    } else if (usage == USAGE_F) {
        feed(shell);
    } else if (usage == USAGE_SPACE) {
        throw_ball(shell);
    } else if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER) {
        pet(shell);
    } else {
        return PDA_APP_NONE;
    }
    pace(shell);
    return redraw_if_changed(shell);
}

/* Drawing. */

/** A small bitmap: rows of up to 8 pixels, bit 7 leftmost. */
static void blit(struct pda_fb *fb, int x, int y, const uint8_t *rows, int h, bool lit) {
    for (int r = 0; r < h; r++) {
        for (int col = 0; col < 8; col++) {
            if (rows[r] & (0x80 >> col)) {
                pda_fb_set(fb, x + col, y + r, lit);
            }
        }
    }
}

static const uint8_t bowl_art[5] = {0x00, 0x54, 0xFE, 0x7C, 0x38};
static const uint8_t yarn[5] = {0x70, 0xA8, 0xD8, 0xA8, 0x70};
static const uint8_t bolt[5] = {0x30, 0x60, 0xF0, 0x60, 0xC0};
static const uint8_t heart[6] = {0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10};
static const uint8_t ball_art[5] = {0x70, 0xD8, 0xA8, 0xD8, 0x70};

static void draw_meters(struct pda_fb *fb, struct pda_rect area, const struct scene *s) {
    const uint8_t *const icons[3] = {bowl_art, yarn, bolt};
    for (int i = 0; i < 3; i++) {
        const int x = area.x + 1 + 23 * i;
        blit(fb, x, area.y + 1, icons[i], 5, true);
        pda_fb_frame(fb, (struct pda_rect){x + 8, area.y + 1, METER_PX + 2, 5}, true);
        pda_fb_fill(fb, (struct pda_rect){x + 9, area.y + 2, s->meters[i], 3}, true);
    }
}

static void draw_window(struct pda_fb *fb, int ox, int oy, uint8_t sky) {
    const struct pda_rect window = {ox + 42, oy + 40, 22, 26};
    pda_fb_frame(fb, window, true);
    pda_fb_frame(fb, (struct pda_rect){window.x + 1, window.y + 1, window.w - 2, window.h - 2},
                 true);
    pda_fb_fill(fb, (struct pda_rect){window.x + 10, window.y, 2, window.h}, true);
    pda_fb_fill(fb, (struct pda_rect){window.x, window.y + 12, window.w, 2}, true);
    pda_fb_fill(fb, (struct pda_rect){window.x - 2, window.y + window.h, window.w + 4, 2}, true);
    const int cx = window.x + 6, cy = window.y + 6;
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            const bool disc = dx * dx + dy * dy <= 10;
            if (sky == 1 && disc) {
                pda_fb_set(fb, cx + dx, cy + dy, true);
            } else if (sky == 2 && disc && (dx + 2) * (dx + 2) + dy * dy > 8) {
                pda_fb_set(fb, cx + dx, cy + dy, true);
            }
        }
    }
    if (sky == 2) {
        pda_fb_set(fb, window.x + 16, window.y + 4, true);
        pda_fb_set(fb, window.x + 14, window.y + 8, true);
    }
}

static void draw_room(struct pda_fb *fb, struct pda_rect area, const struct scene *s) {
    const int ox = area.x, oy = area.y;
    draw_window(fb, ox, oy, s->sky);
    // The cat tree: a top shelf with the bed, a sisal post, a lower shelf.
    for (int i = SHELF; i < LEVEL_COUNT; i++) {
        pda_fb_fill(fb,
                    (struct pda_rect){ox + levels[i].left, oy + levels[i].surface,
                                      levels[i].right - levels[i].left + 1, 3},
                    true);
    }
    const int post_top = levels[TOP].surface + 3, post_bottom = levels[FLOOR].surface;
    pda_fb_frame(fb, (struct pda_rect){ox + 31, oy + post_top, 5, post_bottom - post_top}, true);
    for (int y = post_top + 4; y < post_bottom - 1; y += 3) {
        pda_fb_set(fb, ox + 32, oy + y, true);
        pda_fb_set(fb, ox + 33, oy + y - 1, true);
        pda_fb_set(fb, ox + 34, oy + y - 2, true);
    }
    // The floor.
    const int floor_y = oy + levels[FLOOR].surface;
    pda_fb_fill(fb, (struct pda_rect){ox, floor_y, area.w, 1}, true);
    for (int x = 0; x < area.w; x += 2) {
        pda_fb_set(fb, ox + x + (x / 2) % 2, floor_y + 2, true);
    }
    // The bowl, and a heap of kibble per portion.
    pda_fb_fill(fb, (struct pda_rect){ox + 49, floor_y - 4, 15, 1}, true);
    pda_fb_fill(fb, (struct pda_rect){ox + 50, floor_y - 3, 13, 1}, true);
    pda_fb_fill(fb, (struct pda_rect){ox + 51, floor_y - 2, 11, 1}, true);
    pda_fb_fill(fb, (struct pda_rect){ox + 52, floor_y - 1, 9, 1}, true);
    for (int p = 0; p < s->bowl; p++) {
        for (int x = 51 + 2 * p; x <= 61 - 2 * p; x += 2) {
            pda_fb_set(fb, ox + x + p % 2, floor_y - 5 - p, true);
        }
    }
}

static void draw_cat(struct pda_fb *fb, struct pda_rect area, const struct scene *s) {
    if (s->frame >= CAT_FRAME_COUNT) {
        return;
    }
    for (int r = 0; r < CAT_HEIGHT; r++) {
        const int y = area.y + s->top + r;
        if (y < area.y || y >= area.y + area.h) {
            continue;
        }
        for (int col = 0; col < CAT_WIDTH; col++) {
            const uint32_t bit = 1u << (CAT_WIDTH - 1 - col);
            const int x = area.x + s->x + col;
            if (x < area.x || x >= area.x + area.w || !(pda_cat_masks[s->frame][r] & bit)) {
                continue;
            }
            pda_fb_set(fb, x, y, (pda_cat_sprites[s->frame][r] & bit) != 0);
        }
    }
}

/** The bed's front, over her lowest rows: she lies in it rather than on it. */
static void draw_bed(struct pda_fb *fb, struct pda_rect area) {
    const struct pda_rect bed = {area.x + 1, area.y + levels[TOP].surface - 3, 27, 3};
    pda_fb_fill(fb, bed, false);
    pda_fb_frame(fb, bed, true);
    for (int x = bed.x + 2; x < bed.x + bed.w - 1; x += 2) {
        pda_fb_set(fb, x, bed.y + 1, true);
    }
}

static void draw_bubble(struct pda_fb *fb, struct pda_rect area, const struct scene *s) {
    const bool left = s->frame >= CAT_FACING_LEFT;
    const struct pda_rect bubble = {area.x + s->x + (left ? -2 : CAT_WIDTH - 6), area.y + s->top - 11,
                                    7, 9};
    pda_fb_fill(fb, bubble, false);
    pda_fb_frame(fb, bubble, true);
    pda_fb_fill(fb, (struct pda_rect){bubble.x + 3, bubble.y + 2, 1, 3}, true);
    pda_fb_set(fb, bubble.x + 3, bubble.y + 6, true);
}

static void cat_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    struct scene s;
    scene_of(shell, &s);
    draw_meters(fb, area, &s);
    draw_room(fb, area, &s);
    if (s.ball) {
        const int x = area.x + s.ball_x, y = area.y + s.ball_y;
        pda_fb_fill(fb, (struct pda_rect){x, y, BALL_SIZE, BALL_SIZE}, false);
        blit(fb, x, y, ball_art, BALL_SIZE, true);
    }
    draw_cat(fb, area, &s);
    draw_bed(fb, area);
    if (s.bubble) {
        draw_bubble(fb, area, &s);
    }
    for (int i = 0; i < PDA_CAT_HEARTS; i++) {
        if (s.heart_x[i] != NO_HEART) {
            blit(fb, area.x + s.heart_x[i], area.y + s.heart_y[i], heart, 6, true);
        }
    }
    if (s.laser) {
        const int x = area.x + s.laser_x, y = area.y + s.laser_y;
        pda_fb_invert(fb, (struct pda_rect){x - 1, y, 3, 1});
        pda_fb_invert(fb, (struct pda_rect){x, y - 1, 1, 1});
        pda_fb_invert(fb, (struct pda_rect){x, y + 1, 1, 1});
    }
}

const struct pda_app pda_app_cat_def = {
    .name = "Cat",
    .open = cat_open,
    .key = cat_key,
    .tick = cat_tick,
    .draw = cat_draw,
};
