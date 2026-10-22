/*
 * SPDX-License-Identifier: MIT
 */

#include "cat.h"

#include <stddef.h>

#define NO_FRAME CAT_FRAME_COUNT
#define STAND CAT_WALK_1
#define MIDDLE_X ((CAT_BOX_WIDTH - CAT_WIDTH) / 2)
#define RIGHT_END_X (CAT_BOX_WIDTH - CAT_WIDTH)
#define FOREVER_MS CAT_SLEEP_MS
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static const struct cat_step hop[] = {
    {60, CAT_JUMP, 0, 2},
    {60, CAT_JUMP, 0, 3},
    {60, CAT_FALL, 0, 2},
    {40, CAT_FALL, 0, 1},
};

static const struct cat_step jump[] = {
    {80, CAT_CROUCH, 0, 0}, {60, CAT_JUMP, 0, 2}, {60, CAT_JUMP, 0, 4}, {120, CAT_JUMP, 0, 5},
    {60, CAT_FALL, 0, 4},   {60, CAT_FALL, 0, 2}, {80, CAT_CROUCH, 0, 0},
};

static const struct cat_step duck[] = {
    {400, CAT_CROUCH, 0, 0},
};

static const struct cat_step pounce[] = {
    {100, CAT_CROUCH, 0, 0}, {60, CAT_POUNCE, 2, 3}, {80, CAT_POUNCE, 5, 5},
    {60, CAT_POUNCE, 8, 3},  {60, CAT_FALL, 10, 1},  {100, CAT_CROUCH, 10, 0},
};

static const struct cat_step leap[] = {
    {60, CAT_POUNCE, 2, 2},
    {60, CAT_POUNCE, 5, 4},
    {60, CAT_POUNCE, 8, 2},
    {60, CAT_CROUCH, 8, 0},
};

static const struct cat_step flop[] = {
    {150, CAT_SIT, 0, 0},
    {150, CAT_CROUCH, 0, 0},
};

static const struct cat_step stretch[] = {
    {800, CAT_STRETCH, 0, 0},
};

enum cat_key cat_classify_key(uint16_t usage_page, uint32_t keycode) {
    if (usage_page != 0x07) {
        return CAT_KEY_OTHER;
    }
    switch (keycode) {
    case 0x28: // Enter
    case 0x58: // Keypad Enter
        return CAT_KEY_ENTER;
    case 0x2A: // Backspace
    case 0x4C: // Delete
        return CAT_KEY_BACKSPACE;
    case 0x2C:
        return CAT_KEY_SPACE;
    case 0x4A:
        return CAT_KEY_HOME;
    case 0x4D:
        return CAT_KEY_END;
    case 0x4F:
        return CAT_KEY_RIGHT;
    case 0x50:
        return CAT_KEY_LEFT;
    case 0x51:
        return CAT_KEY_DOWN;
    case 0x52:
        return CAT_KEY_UP;
    case 0x2B: // Tab
    case 0x64: // Non-US backslash
    case 0x67: // Keypad =
    case 0x85: // Keypad comma
    case 0x86: // Keypad equal sign
    case 0xB6: // Keypad (
    case 0xB7: // Keypad )
        return CAT_KEY_TYPING;
    default:
        break;
    }
    const bool letter_or_digit = keycode >= 0x04 && keycode <= 0x27;
    const bool punctuation = keycode >= 0x2D && keycode <= 0x38;
    const bool keypad = keycode >= 0x54 && keycode <= 0x63 && keycode != 0x58;
    return letter_or_digit || punctuation || keypad ? CAT_KEY_TYPING : CAT_KEY_OTHER;
}

/** Time from then to now, or 0 if now is before then. */
static uint32_t since(uint32_t now, uint32_t then) {
    const uint32_t d = now - then;
    return d > INT32_MAX ? 0 : d;
}

static uint32_t later(uint32_t a, uint32_t b) { return since(a, b) > 0 ? a : b; }

static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

static int wrap(int x) {
    return ((x + CAT_WIDTH) % CAT_LAP_PX + CAT_LAP_PX) % CAT_LAP_PX - CAT_WIDTH;
}

static int mod4(int32_t n) { return (int)(((n % 4) + 4) % 4); }

static uint32_t run_steps(const struct cat *cat) {
    const int px = cat->run_px < 0 ? -cat->run_px : cat->run_px;
    return (uint32_t)((px + CAT_DASH_STEP_PX - 1) / CAT_DASH_STEP_PX);
}

static uint32_t script_ms(const struct cat_step *script, size_t len) {
    uint32_t ms = 0;
    for (size_t i = 0; i < len; i++) {
        ms += script[i].ms;
    }
    return ms;
}

static uint32_t action_ms(const struct cat *cat) {
    return run_steps(cat) * CAT_DASH_STEP_MS + script_ms(cat->script, cat->script_len);
}

static bool action_running(const struct cat *cat, uint32_t now) {
    return (cat->script_len > 0 || cat->run_px != 0) &&
           since(now, cat->action_ms) < action_ms(cat);
}

static enum cat_frame facing(enum cat_frame frame, bool left) {
    return left ? (enum cat_frame)(CAT_FACING_LEFT + frame) : frame;
}

/** The view while an action runs; only valid when action_running() is true. */
static struct cat_view action_view(const struct cat *cat, uint32_t now) {
    const int dir = cat->action_left ? -1 : 1;
    uint32_t t = since(now, cat->action_ms);
    const uint32_t steps = run_steps(cat);
    if (t < steps * CAT_DASH_STEP_MS) {
        const uint32_t k = t / CAT_DASH_STEP_MS;
        const int run = cat->run_px < 0 ? -cat->run_px : cat->run_px;
        int moved = (int)(k + 1) * CAT_DASH_STEP_PX;
        moved = moved > run ? run : moved;
        return (struct cat_view){
            .frame = facing((enum cat_frame)(CAT_RUN_0 + k % 4), cat->action_left),
            .x = wrap(cat->from_x + (cat->run_px < 0 ? -moved : moved)),
            .valid_ms = CAT_DASH_STEP_MS - t % CAT_DASH_STEP_MS,
        };
    }
    t -= steps * CAT_DASH_STEP_MS;
    for (size_t i = 0; i < cat->script_len; i++) {
        const struct cat_step *step = &cat->script[i];
        if (t < step->ms) {
            return (struct cat_view){
                .frame = facing((enum cat_frame)step->frame, cat->action_left),
                .x = wrap(cat->from_x + cat->run_px + dir * step->dx),
                .y = step->dy,
                .valid_ms = step->ms - t,
            };
        }
        t -= step->ms;
    }
    return (struct cat_view){.frame = facing(STAND, cat->left), .x = cat->x, .valid_ms = 1};
}

static struct cat_view stalk_view(const struct cat *cat, uint32_t now) {
    const uint32_t t = since(now, cat->stalk_ms);
    switch (cat->stalk) {
    case CAT_MOUSE_LEFT:
    case CAT_MOUSE_RIGHT: {
        const uint32_t k = t / CAT_STALK_STEP_MS;
        const int moved = (int)((k + 1) % CAT_LAP_PX) * CAT_STEP_PX;
        return (struct cat_view){
            .frame = facing(k % 2 ? CAT_STALK_1 : CAT_STALK_0, cat->left),
            .x = wrap(cat->stalk_x + (cat->left ? -moved : moved)),
            .valid_ms = CAT_STALK_STEP_MS - t % CAT_STALK_STEP_MS,
        };
    }
    case CAT_MOUSE_UP:
        return (struct cat_view){
            .frame = facing(CAT_HUNT_UP, cat->left), .x = cat->stalk_x, .valid_ms = FOREVER_MS};
    default:
        return (struct cat_view){
            .frame = facing(CAT_CROUCH, cat->left), .x = cat->stalk_x, .valid_ms = FOREVER_MS};
    }
}

static enum cat_frame pose_frame(const struct cat *cat, uint32_t now, uint32_t *valid_ms) {
    switch (cat->pose) {
    case CAT_POSE_KEYPAD:
        return CAT_SIT_UP;
    case CAT_POSE_SYSTEM:
        return CAT_HUNT;
    case CAT_POSE_LOCKED:
        return CAT_LOAF;
    case CAT_POSE_GAMING: {
        uint32_t still = cat->rest_ms;
        if (cat->rest_frame != NO_FRAME) {
            still += CAT_SETTLE_MS;
        }
        const uint32_t w = since(now, later(still, cat->pose_ms));
        if (w < CAT_WIGGLE_MS) {
            *valid_ms = min_u32(*valid_ms, CAT_WIGGLE_STEP_MS - w % CAT_WIGGLE_STEP_MS);
            return (w / CAT_WIGGLE_STEP_MS) % 2 ? CAT_READY_1 : CAT_READY_0;
        }
        return CAT_READY_0;
    }
    default:
        return STAND;
    }
}

static struct cat_view rest_view(const struct cat *cat, uint32_t now) {
    struct cat_view view = {.x = cat->x};
    const uint32_t idle = since(now, cat->rest_ms);
    enum cat_frame frame;

    if (idle >= CAT_SLEEP_MS) {
        const uint32_t asleep = idle - CAT_SLEEP_MS;
        view.valid_ms = CAT_SNORE_MS - asleep % CAT_SNORE_MS;
        frame = (asleep / CAT_SNORE_MS) % 2 ? CAT_SLEEP_1 : CAT_SLEEP_0;
    } else if (cat->pose == CAT_POSE_NONE) {
        const uint32_t until_sleep = CAT_SLEEP_MS - idle;
        if (idle < CAT_SIT_MS) {
            view.valid_ms = CAT_SIT_MS - idle;
            frame = cat->rest_frame != NO_FRAME ? cat->rest_frame : STAND;
        } else {
            const uint32_t phase = (idle - CAT_SIT_MS) % CAT_BLINK_PERIOD_MS;
            if (phase < CAT_BLINK_PERIOD_MS - CAT_BLINK_MS) {
                view.valid_ms = min_u32(CAT_BLINK_PERIOD_MS - CAT_BLINK_MS - phase, until_sleep);
                frame = CAT_SIT;
            } else {
                view.valid_ms = min_u32(CAT_BLINK_PERIOD_MS - phase, until_sleep);
                frame = CAT_BLINK;
            }
        }
    } else if (cat->rest_frame != NO_FRAME && idle < CAT_SETTLE_MS) {
        view.valid_ms = CAT_SETTLE_MS - idle;
        frame = cat->rest_frame;
    } else {
        view.valid_ms = CAT_SLEEP_MS - idle;
        frame = pose_frame(cat, now, &view.valid_ms);
    }
    view.frame = facing(frame, cat->left);
    return view;
}

struct cat_view cat_view_at(const struct cat *cat, uint32_t now_ms) {
    if (cat->away && !action_running(cat, now_ms)) {
        return (struct cat_view){.frame = NO_FRAME, .x = cat->x, .valid_ms = FOREVER_MS};
    }
    if (cat->stalking) {
        return stalk_view(cat, now_ms);
    }
    if (action_running(cat, now_ms)) {
        return action_view(cat, now_ms);
    }
    return rest_view(cat, now_ms);
}

/** Stops whatever the cat does, leaving it where it is now, and wakes it. */
static void stop(struct cat *cat, uint32_t now) {
    if (cat->stalking) {
        cat->x = stalk_view(cat, now).x;
        cat->stalking = false;
    } else if (action_running(cat, now)) {
        cat->x = action_view(cat, now).x;
    }
    cat->script = NULL;
    cat->script_len = 0;
    cat->run_px = 0;
    cat->rest_ms = now;
}

/** Starts an action: a run of run_px from where the cat is, then the script. */
static void act(struct cat *cat, uint32_t now, int run_px, bool left, const struct cat_step *script,
                size_t script_len) {
    stop(cat, now);
    cat->from_x = cat->x;
    cat->run_px = run_px;
    cat->action_left = left;
    cat->script = script;
    cat->script_len = (uint8_t)script_len;
    cat->action_ms = now;
    cat->left = left;
    cat->rest_ms = now + action_ms(cat);
    cat->rest_frame = NO_FRAME;
    const int dx = script_len > 0 ? script[script_len - 1].dx : 0;
    cat->x = wrap(cat->x + run_px + (left ? -dx : dx));
}

/** Walks, or runs if the last stride was recent; a moonwalk always walks. */
static void stride(struct cat *cat, uint32_t now, bool to_left, bool face_left, bool moonwalk) {
    stop(cat, now);
    const bool fast = !moonwalk && cat->typed && since(now, cat->typed_ms) < CAT_RUN_INTERVAL_MS;
    cat->typed = true;
    cat->typed_ms = now;
    const int px = fast ? CAT_RUN_STEP_PX : CAT_STEP_PX;
    cat->x = wrap(cat->x + (to_left ? -px : px));
    cat->strides += moonwalk ? -1 : 1;
    cat->left = face_left;
    cat->rest_frame = (enum cat_frame)((fast ? CAT_RUN_0 : CAT_WALK_0) + mod4(cat->strides));
}

static void scamper(struct cat *cat, uint32_t now, int to_x, bool left_if_still) {
    stop(cat, now);
    const int run_px = to_x - cat->x;
    act(cat, now, run_px, run_px == 0 ? left_if_still : run_px < 0, NULL, 0);
}

void cat_init(struct cat *cat, int x, uint32_t now_ms) {
    *cat = (struct cat){
        .x = wrap(x),
        .pose = CAT_POSE_NONE,
        .pose_ms = now_ms,
        .rest_ms = now_ms,
        .rest_frame = NO_FRAME,
    };
}

void cat_key_pressed(struct cat *cat, enum cat_key key, uint32_t now_ms) {
    if (cat->away) {
        return;
    }
    switch (cat->pose) {
    case CAT_POSE_LOCKED:
        return;
    case CAT_POSE_GAMING: {
        bool left = cat->left;
        if (key == CAT_KEY_LEFT || key == CAT_KEY_RIGHT) {
            left = key == CAT_KEY_LEFT;
        }
        act(cat, now_ms, 0, left, leap, ARRAY_LEN(leap));
        return;
    }
    default:
        break;
    }

    switch (key) {
    case CAT_KEY_TYPING:
        stride(cat, now_ms, false, false, false);
        break;
    case CAT_KEY_SPACE: {
        stride(cat, now_ms, false, false, false);
        const enum cat_frame frame = cat->rest_frame;
        act(cat, now_ms, 0, false, hop, ARRAY_LEN(hop));
        cat->rest_frame = frame;
        break;
    }
    case CAT_KEY_ENTER:
    case CAT_KEY_UP:
        act(cat, now_ms, 0, cat->left, jump, ARRAY_LEN(jump));
        break;
    case CAT_KEY_BACKSPACE:
        stride(cat, now_ms, true, false, true);
        break;
    case CAT_KEY_LEFT:
        stride(cat, now_ms, true, true, false);
        break;
    case CAT_KEY_RIGHT:
        stride(cat, now_ms, false, false, false);
        break;
    case CAT_KEY_DOWN:
        act(cat, now_ms, 0, cat->left, duck, ARRAY_LEN(duck));
        break;
    case CAT_KEY_HOME:
        scamper(cat, now_ms, 0, true);
        break;
    case CAT_KEY_END:
        scamper(cat, now_ms, RIGHT_END_X, false);
        break;
    case CAT_KEY_OTHER:
        break;
    }
}

void cat_mouse_pressed(struct cat *cat, enum cat_mouse mouse, uint32_t now_ms) {
    if (cat->away || cat->pose == CAT_POSE_LOCKED) {
        return;
    }
    if (mouse == CAT_MOUSE_CLICK) {
        act(cat, now_ms, 0, cat->left, pounce, ARRAY_LEN(pounce));
        return;
    }
    stop(cat, now_ms);
    if (mouse == CAT_MOUSE_LEFT || mouse == CAT_MOUSE_RIGHT) {
        cat->left = mouse == CAT_MOUSE_LEFT;
    }
    cat->stalking = true;
    cat->stalk = mouse;
    cat->stalk_ms = now_ms;
    cat->stalk_x = cat->x;
    cat->rest_frame = NO_FRAME;
}

void cat_mouse_released(struct cat *cat, enum cat_mouse mouse, uint32_t now_ms) {
    if (cat->stalking && cat->stalk == mouse) {
        stop(cat, now_ms);
    }
}

void cat_set_pose(struct cat *cat, enum cat_pose pose, uint32_t now_ms) {
    if (pose == cat->pose) {
        return;
    }
    const enum cat_pose old = cat->pose;
    cat->pose = pose;
    cat->pose_ms = now_ms;
    if (cat->away) {
        return;
    }
    if (pose == CAT_POSE_LOCKED) {
        stop(cat, now_ms);
        const int run_px = MIDDLE_X - cat->x;
        act(cat, now_ms, run_px, run_px == 0 ? cat->left : run_px < 0, flop, ARRAY_LEN(flop));
    } else if (old == CAT_POSE_LOCKED) {
        act(cat, now_ms, 0, cat->left, stretch, ARRAY_LEN(stretch));
    } else if (cat->stalking) {
        stop(cat, now_ms);
    } else {
        cat->rest_ms = later(cat->rest_ms, now_ms);
    }
}

void cat_profile_changed(struct cat *cat, uint32_t now_ms) {
    if (!cat->away) {
        act(cat, now_ms, CAT_LAP_PX, false, NULL, 0);
    }
}

void cat_set_away(struct cat *cat, bool away, uint32_t now_ms) {
    if (away == cat->away) {
        return;
    }
    cat->away = away;
    if (away) {
        stop(cat, now_ms);
        act(cat, now_ms, CAT_BOX_WIDTH - cat->x, false, NULL, 0);
    } else {
        // Off the right edge wraps to off the left edge, so run left from there.
        cat->x = wrap(CAT_BOX_WIDTH);
        act(cat, now_ms, CAT_RETURN_X - CAT_BOX_WIDTH, true, NULL, 0);
    }
}
