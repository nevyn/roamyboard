/*
 * Bricks: Breakout's six rows of bricks, a paddle that Left and Right move while held, and a ball that
 * Space serves. The paddle's edges send the ball off at a steeper angle. Clearing the wall
 * builds a new one and speeds the ball up.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

/** About 30 frames a second, which the nice!view keeps up with. */
#define PLAY_TICK_MS 33
/** A longer gap between ticks is a pause, which does not count as time played. */
#define MAX_STEP_MS 200
#define LIVES 3
#define STATUS_H 10
#define FIELD_W PDA_FB_W
#define FIELD_H (PDA_FB_H - PDA_TITLE_H - STATUS_H)
#define BRICK_W 10
#define BRICK_H 4
#define BRICK_GAP 1
#define BRICKS_TOP 8
#define PADDLE_W 16
#define PADDLE_H 2
#define PADDLE_Y (FIELD_H - 6)
#define BALL 2
#define PADDLE_SPEED (90 * 256)

enum {
    USAGE_SPACE = 0x2C,
};

static int32_t ball_speed(const struct pda_breakout_state *b) { return (40 + 8 * b->level) * 256; }

static void serve_position(struct pda_breakout_state *b) {
    b->serving = true;
    b->ball_x_q8 = b->paddle_q8 + (PADDLE_W / 2 - BALL / 2) * 256;
    b->ball_y_q8 = (PADDLE_Y - BALL) * 256;
}

static void build_wall(struct pda_breakout_state *b) {
    for (int r = 0; r < PDA_BREAKOUT_ROWS; r++) {
        b->bricks[r] = (1 << PDA_BREAKOUT_COLS) - 1;
    }
}

static void start(struct pda_shell *shell) {
    struct pda_breakout_state *b = &shell->breakout;
    memset(b, 0, sizeof(*b));
    b->started = true;
    b->lives = LIVES;
    b->paddle_q8 = (FIELD_W - PADDLE_W) / 2 * 256;
    b->last_ms = shell->now_ms;
    build_wall(b);
    serve_position(b);
    shell->tick_ms = PLAY_TICK_MS;
}

static int brick_x(int c) {
    const int wall = PDA_BREAKOUT_COLS * (BRICK_W + BRICK_GAP) - BRICK_GAP;
    return (FIELD_W - wall) / 2 + c * (BRICK_W + BRICK_GAP);
}

static int brick_y(int r) { return BRICKS_TOP + r * (BRICK_H + BRICK_GAP); }

/** The brick that a ball at (bx, by) touches, as row * PDA_BREAKOUT_COLS + column, or -1. */
static int brick_at(const struct pda_breakout_state *b, int bx, int by) {
    for (int r = 0; r < PDA_BREAKOUT_ROWS; r++) {
        for (int c = 0; c < PDA_BREAKOUT_COLS; c++) {
            if (!(b->bricks[r] & (1 << c))) {
                continue;
            }
            const int x = brick_x(c), y = brick_y(r);
            if (bx + BALL > x && bx < x + BRICK_W && by + BALL > y && by < y + BRICK_H) {
                return r * PDA_BREAKOUT_COLS + c;
            }
        }
    }
    return -1;
}

static bool wall_cleared(const struct pda_breakout_state *b) {
    for (int r = 0; r < PDA_BREAKOUT_ROWS; r++) {
        if (b->bricks[r]) {
            return false;
        }
    }
    return true;
}

static void lose_ball(struct pda_shell *shell) {
    struct pda_breakout_state *b = &shell->breakout;
    if (--b->lives > 0) {
        serve_position(b);
        return;
    }
    b->over = true;
    shell->tick_ms = 0;
    if (b->score > shell->scores.breakout) {
        shell->scores.breakout = b->score;
        shell->scores_changed = true;
    }
}

static void move_ball(struct pda_shell *shell, uint32_t dt) {
    struct pda_breakout_state *b = &shell->breakout;
    const int32_t old_x = b->ball_x_q8, old_y = b->ball_y_q8;
    b->ball_x_q8 += b->vx_q8 * (int32_t)dt / 1000;
    b->ball_y_q8 += b->vy_q8 * (int32_t)dt / 1000;

    if (b->ball_x_q8 < 0 || b->ball_x_q8 > (FIELD_W - BALL) * 256) {
        b->vx_q8 = -b->vx_q8;
        b->ball_x_q8 = b->ball_x_q8 < 0 ? 0 : (FIELD_W - BALL) * 256;
    }
    if (b->ball_y_q8 < 0) {
        b->vy_q8 = -b->vy_q8;
        b->ball_y_q8 = 0;
    }

    const int bx = b->ball_x_q8 >> 8, by = b->ball_y_q8 >> 8;
    const int brick = brick_at(b, bx, by);
    if (brick >= 0) {
        const int r = brick / PDA_BREAKOUT_COLS, c = brick % PDA_BREAKOUT_COLS;
        // Bounce off the side that the ball crossed: sideways only if moving sideways alone
        // reached the brick, otherwise across its top or bottom.
        if (brick_at(b, old_x >> 8, by) < 0 && brick_at(b, bx, old_y >> 8) >= 0) {
            b->vx_q8 = -b->vx_q8;
            b->ball_x_q8 = old_x;
        } else {
            b->vy_q8 = -b->vy_q8;
            b->ball_y_q8 = old_y;
        }
        b->bricks[r] &= (uint8_t)~(1 << c);
        b->score += (uint32_t)(PDA_BREAKOUT_ROWS - r);
        if (wall_cleared(b)) {
            b->level++;
            build_wall(b);
            serve_position(b);
        }
        return;
    }

    const int px = b->paddle_q8 >> 8;
    if (b->vy_q8 > 0 && by + BALL >= PADDLE_Y && (old_y >> 8) + BALL <= PADDLE_Y &&
        bx + BALL > px && bx < px + PADDLE_W) {
        // -4 at the left edge to 4 at the right: the edges send the ball off steeper.
        const int offset = (bx + BALL / 2 - px - PADDLE_W / 2) * 8 / PADDLE_W;
        const int32_t speed = ball_speed(b);
        b->vx_q8 = speed * offset / 5;
        b->vy_q8 = -(speed - (offset < 0 ? -offset : offset) * speed / 10);
        b->ball_y_q8 = (PADDLE_Y - BALL) * 256;
    } else if (by > FIELD_H) {
        lose_ball(shell);
    }
}

static void breakout_open(struct pda_shell *shell) {
    struct pda_breakout_state *b = &shell->breakout;
    shell->tick_ms = b->started && !b->over ? PLAY_TICK_MS : 0;
    b->last_ms = shell->now_ms;
    b->left_held = b->right_held = false;
}

static enum pda_app_result breakout_key(struct pda_shell *shell, uint8_t usage,
                                        uint8_t implicit_mods, bool pressed) {
    (void)implicit_mods;
    struct pda_breakout_state *b = &shell->breakout;
    if (pressed && usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    if (!b->started || b->over) {
        if (pressed &&
            (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE)) {
            start(shell);
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    switch (pda_key_dir(usage)) {
    case PDA_DIR_LEFT:
        b->left_held = pressed;
        return PDA_APP_NONE;
    case PDA_DIR_RIGHT:
        b->right_held = pressed;
        return PDA_APP_NONE;
    default:
        break;
    }
    if (pressed && usage == USAGE_SPACE && b->serving) {
        b->serving = false;
        b->vx_q8 = ball_speed(b) / 3;
        b->vy_q8 = -ball_speed(b);
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result breakout_tick(struct pda_shell *shell) {
    struct pda_breakout_state *b = &shell->breakout;
    uint32_t dt = shell->now_ms - b->last_ms;
    b->last_ms = shell->now_ms;
    if (dt > MAX_STEP_MS) {
        // Back from a pause, which does not count as time played; keys held when focus went
        // were released elsewhere.
        dt = PLAY_TICK_MS;
        b->left_held = b->right_held = false;
    }
    if (!b->started || b->over) {
        return PDA_APP_NONE;
    }
    const int steer = (int)b->right_held - (int)b->left_held;
    b->paddle_q8 += steer * PADDLE_SPEED * (int32_t)dt / 1000;
    if (b->paddle_q8 < 0) {
        b->paddle_q8 = 0;
    } else if (b->paddle_q8 > (FIELD_W - PADDLE_W) * 256) {
        b->paddle_q8 = (FIELD_W - PADDLE_W) * 256;
    }
    // Redraws only when something moved by a whole pixel.
    const int paddle_px = (int)(b->paddle_q8 >> 8);
    const int ball_x = (int)(b->ball_x_q8 >> 8), ball_y = (int)(b->ball_y_q8 >> 8);
    const uint32_t score = b->score;
    const uint8_t lives = b->lives;
    if (b->serving) {
        b->ball_x_q8 = b->paddle_q8 + (PADDLE_W / 2 - BALL / 2) * 256;
    } else {
        // Small steps, so that the ball cannot pass through a brick between two ticks.
        for (uint32_t done = 0; done < dt && !b->over && !b->serving;) {
            const uint32_t step = dt - done > 10 ? 10 : dt - done;
            move_ball(shell, step);
            done += step;
        }
    }
    const bool moved = paddle_px != (int)(b->paddle_q8 >> 8) ||
                       ball_x != (int)(b->ball_x_q8 >> 8) || ball_y != (int)(b->ball_y_q8 >> 8) ||
                       score != b->score || lives != b->lives || b->over;
    return moved ? PDA_APP_REDRAW : PDA_APP_NONE;
}

static void breakout_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_breakout_state *b = &shell->breakout;
    char text[24];
    snprintf(text, sizeof(text), "%lu", (unsigned long)b->score);
    pda_text_draw(fb, area.x + 1, area.y, 40, text, PDA_FONT_SMALL, true);
    for (int i = 0; i < LIVES; i++) {
        const struct pda_rect life = {area.x + area.w - 6 * (LIVES - i), area.y + 2, 4, 4};
        if (i < b->lives) {
            pda_fb_fill(fb, life, true);
        } else {
            pda_fb_frame(fb, life, true);
        }
    }
    const int top = area.y + STATUS_H;
    pda_fb_fill(fb, (struct pda_rect){area.x, top - 1, area.w, 1}, true);

    if (b->started) {
        for (int r = 0; r < PDA_BREAKOUT_ROWS; r++) {
            for (int c = 0; c < PDA_BREAKOUT_COLS; c++) {
                if (b->bricks[r] & (1 << c)) {
                    pda_fb_fill(fb, (struct pda_rect){brick_x(c), top + brick_y(r), BRICK_W,
                                                      BRICK_H}, true);
                }
            }
        }
        pda_fb_fill(fb, (struct pda_rect){b->paddle_q8 >> 8, top + PADDLE_Y, PADDLE_W, PADDLE_H},
                    true);
        if (!b->over) {
            pda_fb_fill(fb, (struct pda_rect){b->ball_x_q8 >> 8, top + (b->ball_y_q8 >> 8), BALL,
                                              BALL}, true);
        }
    }

    char best[24];
    snprintf(best, sizeof(best), "best %lu", (unsigned long)shell->scores.breakout);
    if (!b->started) {
        const char *const lines[3] = {"Bricks", best, "? help"};
        pda_game_banner(fb, area, area.y + 50, lines);
    } else if (b->over) {
        const char *const lines[3] = {"game over", best, "? help"};
        pda_game_banner(fb, area, area.y + 50, lines);
    } else if (shell->mode == ROAMYBOARD_PDA_OFF) {
        const char *const lines[3] = {"paused", NULL, NULL};
        pda_game_banner(fb, area, area.y + 60, lines);
    }
}

static const char breakout_help[] =
    "Space, Return\tStart.\n"
    "Left, Right (A D, H L)\tMove the paddle while held.\n"
    "Space\tServe the ball.\n"
    "The paddle's edges send the ball off steeper. Three balls.\n"
    "Esc\tBack to the home screen; the game waits.";

const struct pda_app pda_app_breakout_def = {
    .name = "Bricks",
    .open = breakout_open,
    .key = breakout_key,
    .tick = breakout_tick,
    .focused_ticks = true,
    .draw = breakout_draw,
    .help = breakout_help,
};
