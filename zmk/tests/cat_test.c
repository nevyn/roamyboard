/*
 * Host tests for zmk/src/display/cat.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cat.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        long long a_ = (long long)(a), b_ = (long long)(b);                                        \
        if (a_ != b_) {                                                                            \
            fprintf(stderr, "%s:%d: CHECK_EQ failed in %s: %s == %lld, expected %s == %lld\n",     \
                    __FILE__, __LINE__, __func__, #a, a_, #b, b_);                                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define MIDDLE_X ((CAT_BOX_WIDTH - CAT_WIDTH) / 2)
#define RIGHT_END_X (CAT_BOX_WIDTH - CAT_WIDTH)
/* A start time near the uint32_t wrap, so that every test also crosses it. */
#define T0 (UINT32_MAX - 5000u)

static bool faces_left(struct cat_view v) { return v.frame >= CAT_FACING_LEFT; }

static enum cat_frame upright(struct cat_view v) {
    return faces_left(v) ? (enum cat_frame)(v.frame - CAT_FACING_LEFT) : v.frame;
}

static bool in_range(struct cat_view v) {
    return v.x >= -CAT_WIDTH && v.x < CAT_BOX_WIDTH && v.y >= 0 && v.y <= CAT_MAX_RISE_PX &&
           v.valid_ms > 0;
}

/** Views at every change from t to end, at most max of them; returns how many. */
static int timeline(const struct cat *cat, uint32_t t, uint32_t end, struct cat_view *views,
                    uint32_t *times, int max) {
    int n = 0;
    while (n < max && (int32_t)(end - t) > 0) {
        const struct cat_view v = cat_view_at(cat, t);
        CHECK(in_range(v));
        views[n] = v;
        if (times) {
            times[n] = t;
        }
        n++;
        t += v.valid_ms;
    }
    return n;
}

static void test_classify(void) {
    CHECK_EQ(cat_classify_key(0x07, 0x04), CAT_KEY_TYPING); /* A */
    CHECK_EQ(cat_classify_key(0x07, 0x1D), CAT_KEY_TYPING); /* Z */
    CHECK_EQ(cat_classify_key(0x07, 0x1E), CAT_KEY_TYPING); /* 1 */
    CHECK_EQ(cat_classify_key(0x07, 0x27), CAT_KEY_TYPING); /* 0 */
    CHECK_EQ(cat_classify_key(0x07, 0x2D), CAT_KEY_TYPING); /* - */
    CHECK_EQ(cat_classify_key(0x07, 0x38), CAT_KEY_TYPING); /* / */
    CHECK_EQ(cat_classify_key(0x07, 0x2B), CAT_KEY_TYPING); /* Tab */
    CHECK_EQ(cat_classify_key(0x07, 0x54), CAT_KEY_TYPING); /* Keypad / */
    CHECK_EQ(cat_classify_key(0x07, 0x57), CAT_KEY_TYPING); /* Keypad + */
    CHECK_EQ(cat_classify_key(0x07, 0x59), CAT_KEY_TYPING); /* Keypad 1 */
    CHECK_EQ(cat_classify_key(0x07, 0x62), CAT_KEY_TYPING); /* Keypad 0 */
    CHECK_EQ(cat_classify_key(0x07, 0x63), CAT_KEY_TYPING); /* Keypad . */
    CHECK_EQ(cat_classify_key(0x07, 0x2C), CAT_KEY_SPACE);
    CHECK_EQ(cat_classify_key(0x07, 0x28), CAT_KEY_ENTER);
    CHECK_EQ(cat_classify_key(0x07, 0x58), CAT_KEY_ENTER); /* Keypad Enter */
    CHECK_EQ(cat_classify_key(0x07, 0x2A), CAT_KEY_BACKSPACE);
    CHECK_EQ(cat_classify_key(0x07, 0x4C), CAT_KEY_BACKSPACE); /* Delete */
    CHECK_EQ(cat_classify_key(0x07, 0x50), CAT_KEY_LEFT);
    CHECK_EQ(cat_classify_key(0x07, 0x4F), CAT_KEY_RIGHT);
    CHECK_EQ(cat_classify_key(0x07, 0x52), CAT_KEY_UP);
    CHECK_EQ(cat_classify_key(0x07, 0x51), CAT_KEY_DOWN);
    CHECK_EQ(cat_classify_key(0x07, 0x4A), CAT_KEY_HOME);
    CHECK_EQ(cat_classify_key(0x07, 0x4D), CAT_KEY_END);

    CHECK_EQ(cat_classify_key(0x07, 0x29), CAT_KEY_OTHER); /* Esc */
    CHECK_EQ(cat_classify_key(0x07, 0x39), CAT_KEY_OTHER); /* Caps Lock */
    CHECK_EQ(cat_classify_key(0x07, 0x3A), CAT_KEY_OTHER); /* F1 */
    CHECK_EQ(cat_classify_key(0x07, 0x45), CAT_KEY_OTHER); /* F12 */
    CHECK_EQ(cat_classify_key(0x07, 0x46), CAT_KEY_OTHER); /* Print Screen */
    CHECK_EQ(cat_classify_key(0x07, 0x49), CAT_KEY_OTHER); /* Insert */
    CHECK_EQ(cat_classify_key(0x07, 0x4B), CAT_KEY_OTHER); /* Page Up */
    CHECK_EQ(cat_classify_key(0x07, 0x53), CAT_KEY_OTHER); /* Num Lock */
    for (uint32_t mod = 0xE0; mod <= 0xE7; mod++) {
        CHECK_EQ(cat_classify_key(0x07, mod), CAT_KEY_OTHER);
    }
    CHECK_EQ(cat_classify_key(0x0C, 0xE9), CAT_KEY_OTHER); /* Volume Up, consumer page */
    CHECK_EQ(cat_classify_key(0x0C, 0x04), CAT_KEY_OTHER); /* "A" on another page */
}

static void test_walk(void) {
    struct cat cat;
    cat_init(&cat, 10, T0);
    uint32_t t = T0;
    for (int i = 1; i <= 5; i++) {
        t += 300;
        cat_key_pressed(&cat, CAT_KEY_TYPING, t);
        const struct cat_view v = cat_view_at(&cat, t);
        CHECK_EQ(v.x, 10 + i * CAT_STEP_PX);
        CHECK_EQ(v.y, 0);
        CHECK_EQ(v.frame, CAT_WALK_0 + i % 4);
    }
}

static void test_gait(void) {
    struct cat cat;
    cat_init(&cat, 0, T0);
    uint32_t t = T0 + 1000;
    cat_key_pressed(&cat, CAT_KEY_TYPING, t);
    CHECK_EQ(cat_view_at(&cat, t).x, CAT_STEP_PX);

    /* Fast typing runs, with longer steps. */
    int x = CAT_STEP_PX;
    for (int i = 0; i < 4; i++) {
        t += CAT_RUN_INTERVAL_MS - 50;
        cat_key_pressed(&cat, CAT_KEY_TYPING, t);
        x += CAT_RUN_STEP_PX;
        const struct cat_view v = cat_view_at(&cat, t);
        CHECK_EQ(v.x, x);
        CHECK(v.frame >= CAT_RUN_0 && v.frame <= CAT_RUN_3);
    }

    /* A slower key walks again. */
    t += CAT_RUN_INTERVAL_MS + 50;
    cat_key_pressed(&cat, CAT_KEY_TYPING, t);
    const struct cat_view v = cat_view_at(&cat, t);
    CHECK_EQ(v.x, x + CAT_STEP_PX);
    CHECK(v.frame >= CAT_WALK_0 && v.frame <= CAT_WALK_3);
}

static void test_moonwalk(void) {
    struct cat cat;
    cat_init(&cat, 30, T0);
    uint32_t t = T0;
    cat_key_pressed(&cat, CAT_KEY_TYPING, t += 300);
    const enum cat_frame first = cat_view_at(&cat, t).frame;
    enum cat_frame previous = first;
    for (int i = 1; i <= 4; i++) {
        /* Fast backspaces still walk: a moonwalk never runs. */
        cat_key_pressed(&cat, CAT_KEY_BACKSPACE, t += 100);
        const struct cat_view v = cat_view_at(&cat, t);
        CHECK_EQ(v.x, 32 - i * CAT_STEP_PX);
        CHECK(!faces_left(v));
        CHECK_EQ(v.frame, CAT_WALK_0 + (previous - CAT_WALK_0 + 3) % 4);
        previous = v.frame;
    }
    CHECK_EQ(previous, first);
}

static void test_arrows(void) {
    struct cat cat;
    cat_init(&cat, 30, T0);
    uint32_t t = T0;

    cat_key_pressed(&cat, CAT_KEY_LEFT, t += 300);
    struct cat_view v = cat_view_at(&cat, t);
    CHECK(faces_left(v));
    CHECK_EQ(v.x, 30 - CAT_STEP_PX);

    cat_key_pressed(&cat, CAT_KEY_RIGHT, t += 300);
    v = cat_view_at(&cat, t);
    CHECK(!faces_left(v));
    CHECK_EQ(v.x, 30);

    /* Up jumps in place, to CAT_MAX_RISE_PX and back down. */
    cat_key_pressed(&cat, CAT_KEY_UP, t += 300);
    struct cat_view views[32];
    int n = timeline(&cat, t, t + 1000, views, NULL, 32);
    int top = 0;
    for (int i = 0; i < n; i++) {
        CHECK_EQ(views[i].x, 30);
        top = views[i].y > top ? views[i].y : top;
    }
    CHECK_EQ(top, CAT_MAX_RISE_PX);
    CHECK_EQ(views[n - 1].y, 0);
    CHECK_EQ(views[n - 1].frame, CAT_WALK_1);

    /* Enter jumps the same way, facing left if the cat does. */
    cat_key_pressed(&cat, CAT_KEY_LEFT, t += 1000);
    cat_key_pressed(&cat, CAT_KEY_ENTER, t += 300);
    v = cat_view_at(&cat, t + 200);
    CHECK(faces_left(v));
    CHECK(v.y > 0);

    /* Down crouches briefly. */
    cat_key_pressed(&cat, CAT_KEY_DOWN, t += 1000);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_CROUCH);
    CHECK_EQ(upright(cat_view_at(&cat, t + 1000)), CAT_WALK_1);
}

static void test_space_hops(void) {
    struct cat cat;
    cat_init(&cat, 0, T0);
    uint32_t t = T0;
    cat_key_pressed(&cat, CAT_KEY_SPACE, t += 300);
    struct cat_view views[16];
    const int n = timeline(&cat, t, t + 1000, views, NULL, 16);
    int top = 0;
    for (int i = 0; i < n; i++) {
        CHECK_EQ(views[i].x, CAT_STEP_PX);
        top = views[i].y > top ? views[i].y : top;
    }
    CHECK(top > 0 && top < CAT_MAX_RISE_PX);
    /* The hop lands back on the stride's walk frame. */
    CHECK_EQ(views[n - 1].frame, CAT_WALK_1);
    CHECK_EQ(views[n - 1].y, 0);
}

static void test_home_end(void) {
    struct cat cat;
    cat_init(&cat, 30, T0);
    uint32_t t = T0 + 100;
    cat_key_pressed(&cat, CAT_KEY_END, t);
    struct cat_view views[32];
    int n = timeline(&cat, t, t + 1000, views, NULL, 32);
    CHECK(n > 2);
    for (int i = 1; i < n; i++) {
        CHECK(views[i].x >= views[i - 1].x);
    }
    CHECK(upright(views[0]) >= CAT_RUN_0 && upright(views[0]) <= CAT_RUN_3);
    CHECK_EQ(views[n - 1].x, RIGHT_END_X);
    CHECK(!faces_left(views[n - 1]));

    cat_key_pressed(&cat, CAT_KEY_HOME, t += 1000);
    n = timeline(&cat, t, t + 1000, views, NULL, 32);
    for (int i = 1; i < n; i++) {
        CHECK(views[i].x <= views[i - 1].x);
    }
    CHECK_EQ(views[n - 1].x, 0);
    CHECK(faces_left(views[n - 1]));
}

/** Walks the cat a whole lap with key, checking that it crops out at one edge and back in. */
static void check_lap(enum cat_key key, int dir) {
    struct cat cat;
    cat_init(&cat, dir > 0 ? RIGHT_END_X : 0, T0);
    uint32_t t = T0;
    bool cropped_out = false, wrapped = false, cropped_in = false;
    int x = cat_view_at(&cat, t).x;
    for (int i = 0; i < CAT_LAP_PX / CAT_STEP_PX; i++) {
        cat_key_pressed(&cat, key, t += 300);
        const struct cat_view v = cat_view_at(&cat, t);
        CHECK(in_range(v));
        if (dir > 0) {
            cropped_out |= v.x > RIGHT_END_X;
            wrapped |= v.x < x;
            cropped_in |= wrapped && v.x < 0 && v.x > -CAT_WIDTH;
        } else {
            cropped_out |= v.x < 0;
            wrapped |= v.x > x;
            cropped_in |= wrapped && v.x > RIGHT_END_X;
        }
        x = v.x;
    }
    CHECK(cropped_out);
    CHECK(wrapped);
    CHECK(cropped_in);
    /* A whole lap brings the cat back where it started. */
    CHECK_EQ(x, dir > 0 ? RIGHT_END_X : 0);
}

static void test_wrap(void) {
    check_lap(CAT_KEY_TYPING, 1);
    check_lap(CAT_KEY_LEFT, -1);
    check_lap(CAT_KEY_BACKSPACE, -1);
}

static void test_keypad_pose(void) {
    struct cat cat;
    cat_init(&cat, 20, T0);
    uint32_t t = T0 + 100;
    cat_set_pose(&cat, CAT_POSE_KEYPAD, t);
    CHECK_EQ(cat_view_at(&cat, t).frame, CAT_SIT_UP);
    cat_key_pressed(&cat, CAT_KEY_TYPING, t += 100);
    CHECK_EQ(cat_view_at(&cat, t).frame, CAT_WALK_1);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SETTLE_MS).frame, CAT_SIT_UP);
    /* The pose holds until the cat falls asleep. */
    CHECK_EQ(cat_view_at(&cat, t + CAT_SLEEP_MS - 1).frame, CAT_SIT_UP);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SLEEP_MS).frame, CAT_SLEEP_0);
    cat_set_pose(&cat, CAT_POSE_NONE, t += CAT_SLEEP_MS + 10);
    CHECK_EQ(cat_view_at(&cat, t).frame, CAT_WALK_1);
}

static void test_system_pose(void) {
    struct cat cat;
    cat_init(&cat, 20, T0);
    uint32_t t = T0 + 100;
    cat_set_pose(&cat, CAT_POSE_SYSTEM, t);
    CHECK_EQ(cat_view_at(&cat, t).frame, CAT_HUNT);

    /* Holding a mouse-move key stalks that way, a step at a time, until released. */
    cat_mouse_pressed(&cat, CAT_MOUSE_RIGHT, t += 100);
    struct cat_view v = cat_view_at(&cat, t);
    CHECK_EQ(v.x, 20 + CAT_STEP_PX);
    CHECK(v.frame == CAT_STALK_0 || v.frame == CAT_STALK_1);
    v = cat_view_at(&cat, t + 3 * CAT_STALK_STEP_MS);
    CHECK_EQ(v.x, 20 + 4 * CAT_STEP_PX);
    cat_mouse_released(&cat, CAT_MOUSE_RIGHT, t += 3 * CAT_STALK_STEP_MS);
    v = cat_view_at(&cat, t + 5000);
    CHECK_EQ(v.frame, CAT_HUNT);
    CHECK_EQ(v.x, 20 + 4 * CAT_STEP_PX);

    cat_mouse_pressed(&cat, CAT_MOUSE_LEFT, t += 100);
    v = cat_view_at(&cat, t);
    CHECK(faces_left(v));
    CHECK_EQ(v.x, 20 + 3 * CAT_STEP_PX);
    cat_mouse_released(&cat, CAT_MOUSE_LEFT, t += 10);

    cat_mouse_pressed(&cat, CAT_MOUSE_UP, t += 100);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_HUNT_UP);
    /* Releasing another key does not end the stalk. */
    cat_mouse_released(&cat, CAT_MOUSE_DOWN, t += 10);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_HUNT_UP);
    cat_mouse_released(&cat, CAT_MOUSE_UP, t += 10);
    cat_mouse_pressed(&cat, CAT_MOUSE_DOWN, t += 100);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_CROUCH);
    cat_mouse_released(&cat, CAT_MOUSE_DOWN, t += 100);

    /* A click pounces forward, through the air. */
    const int x = cat_view_at(&cat, t).x;
    cat_mouse_pressed(&cat, CAT_MOUSE_CLICK, t += 100);
    cat_mouse_released(&cat, CAT_MOUSE_CLICK, t + 20);
    struct cat_view views[16];
    const int n = timeline(&cat, t, t + 1000, views, NULL, 16);
    bool flew = false;
    for (int i = 0; i < n; i++) {
        flew |= upright(views[i]) == CAT_POUNCE && views[i].y > 0;
    }
    CHECK(flew);
    CHECK_EQ(views[n - 1].x, x - 10);
    CHECK_EQ(upright(views[n - 1]), CAT_HUNT);
}

static void test_gaming_pose(void) {
    struct cat cat;
    cat_init(&cat, 20, T0);
    uint32_t t = T0 + 100;
    cat_set_pose(&cat, CAT_POSE_GAMING, t);
    struct cat_view views[64];
    uint32_t times[64];
    int n = timeline(&cat, t, t + 10000, views, times, 64);
    /* The wiggle alternates, then settles: no more redraws until the cat sleeps. */
    CHECK(n >= 4);
    CHECK_EQ(views[0].frame, CAT_READY_0);
    CHECK_EQ(views[1].frame, CAT_READY_1);
    CHECK_EQ(views[n - 1].frame, CAT_READY_0);
    CHECK(times[n - 1] - t >= CAT_WIGGLE_MS);
    CHECK(times[n - 1] - t < CAT_WIGGLE_MS + CAT_WIGGLE_STEP_MS);

    /* Every key leaps forward, even ones that do not move the cat elsewhere. */
    cat_key_pressed(&cat, CAT_KEY_OTHER, t += 10000);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_POUNCE);
    n = timeline(&cat, t, t + 400, views, NULL, 64);
    CHECK_EQ(views[n - 1].x, 28);
    /* Left turns and leaps that way. */
    cat_key_pressed(&cat, CAT_KEY_LEFT, t += 3000);
    n = timeline(&cat, t, t + 400, views, NULL, 64);
    CHECK(faces_left(views[0]));
    CHECK_EQ(views[n - 1].x, 20);
    /* After the leap, it wiggles again. */
    CHECK_EQ(cat_view_at(&cat, t + 400).frame, CAT_FACING_LEFT + CAT_READY_1);
}

static void test_locked_pose(void) {
    struct cat cat;
    cat_init(&cat, 50, T0);
    uint32_t t = T0 + 100;
    cat_set_pose(&cat, CAT_POSE_LOCKED, t);
    struct cat_view views[32];
    int n = timeline(&cat, t, t + 3000, views, NULL, 32);
    CHECK(upright(views[0]) >= CAT_RUN_0 && upright(views[0]) <= CAT_RUN_3);
    CHECK_EQ(upright(views[n - 1]), CAT_LOAF);
    CHECK_EQ(views[n - 1].x, MIDDLE_X);

    /* Keys and mouse keys do nothing. */
    cat_key_pressed(&cat, CAT_KEY_TYPING, t += 3000);
    cat_key_pressed(&cat, CAT_KEY_END, t);
    cat_mouse_pressed(&cat, CAT_MOUSE_CLICK, t);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_LOAF);
    CHECK_EQ(cat_view_at(&cat, t).x, MIDDLE_X);

    /* Leaving the layer, the cat stretches and gets up. */
    cat_set_pose(&cat, CAT_POSE_NONE, t += 100);
    CHECK_EQ(upright(cat_view_at(&cat, t)), CAT_STRETCH);
    CHECK_EQ(upright(cat_view_at(&cat, t + 1000)), CAT_WALK_1);
    CHECK_EQ(cat_view_at(&cat, t + 1000).x, MIDDLE_X);
}

static void test_profile_dash(void) {
    struct cat cat;
    cat_init(&cat, 30, T0);
    uint32_t t = T0 + 100;
    cat_key_pressed(&cat, CAT_KEY_LEFT, t);
    cat_profile_changed(&cat, t += 500);
    struct cat_view views[64];
    uint32_t times[64];
    const int n = timeline(&cat, t, t + 2000, views, times, 64);
    bool off_right = false, in_left = false;
    for (int i = 0; i < n - 1; i++) {
        CHECK(!faces_left(views[i]));
        off_right |= views[i].x > RIGHT_END_X;
        in_left |= off_right && views[i].x < 0;
    }
    CHECK(off_right);
    CHECK(in_left);
    CHECK_EQ(views[n - 1].x, 28);
    CHECK(times[n - 1] - t < 1000);
}

static void test_away(void) {
    struct cat cat;
    cat_init(&cat, 10, T0);
    uint32_t t = T0 + 100;
    cat_set_away(&cat, true, t);
    struct cat_view views[64];
    uint32_t times[64];
    int n = timeline(&cat, t, t + 2000, views, times, 64);
    for (int i = 0; i < n - 1; i++) {
        CHECK(!faces_left(views[i]));
        CHECK(views[i].x >= 10 || views[i].x == -CAT_WIDTH);
    }
    CHECK_EQ(views[n - 1].frame, CAT_FRAME_COUNT);
    CHECK(times[n - 1] - t < 1000);

    /* Away, keys, mouse keys, poses and profile changes leave the cat away. */
    t += 2000;
    cat_key_pressed(&cat, CAT_KEY_SPACE, t);
    cat_mouse_pressed(&cat, CAT_MOUSE_LEFT, t);
    cat_set_pose(&cat, CAT_POSE_KEYPAD, t);
    cat_profile_changed(&cat, t);
    CHECK_EQ(cat_view_at(&cat, t + 10).frame, CAT_FRAME_COUNT);
    cat_set_away(&cat, true, t);
    CHECK_EQ(cat_view_at(&cat, t + 10).frame, CAT_FRAME_COUNT);

    /* Back, it runs in from the right edge facing left, then takes the pose. */
    cat_mouse_released(&cat, CAT_MOUSE_LEFT, t += 100);
    cat_set_away(&cat, false, t += 100);
    n = timeline(&cat, t, t + 2000, views, times, 64);
    CHECK(views[0].x > CAT_RETURN_X);
    for (int i = 0; i < n; i++) {
        CHECK(faces_left(views[i]));
        CHECK(views[i].x >= CAT_RETURN_X);
    }
    CHECK_EQ(views[n - 1].x, CAT_RETURN_X);
    CHECK_EQ(upright(cat_view_at(&cat, t + 1500)), CAT_SIT_UP);
}

static void test_idle(void) {
    struct cat cat;
    cat_init(&cat, 0, T0);
    const uint32_t t = T0 + 100;
    cat_key_pressed(&cat, CAT_KEY_TYPING, t);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SIT_MS - 1).frame, CAT_WALK_1);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SIT_MS).frame, CAT_SIT);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SIT_MS + CAT_BLINK_PERIOD_MS - CAT_BLINK_MS).frame,
             CAT_BLINK);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SIT_MS + CAT_BLINK_PERIOD_MS).frame, CAT_SIT);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SLEEP_MS).frame, CAT_SLEEP_0);
    CHECK_EQ(cat_view_at(&cat, t + CAT_SLEEP_MS + CAT_SNORE_MS).frame, CAT_SLEEP_1);

    /* Asleep, the screen redraws once per CAT_SNORE_MS and no more. */
    struct cat_view views[16];
    const int n = timeline(&cat, t + CAT_SLEEP_MS, t + CAT_SLEEP_MS + 10000, views, NULL, 16);
    CHECK_EQ(n, 10);

    /* A sleeping cat that faces left keeps facing left. */
    cat_key_pressed(&cat, CAT_KEY_LEFT, t + 50000);
    CHECK_EQ(cat_view_at(&cat, t + 50000 + CAT_SLEEP_MS).frame, CAT_FACING_LEFT + CAT_SLEEP_0);
}

/** Every key-driven view lasts at least as long as the glue's 40 ms between redraws. */
static void test_steps_last_a_frame(void) {
    const enum cat_key keys[] = {CAT_KEY_SPACE, CAT_KEY_ENTER, CAT_KEY_DOWN, CAT_KEY_HOME,
                                 CAT_KEY_END};
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        struct cat cat;
        cat_init(&cat, 30, T0);
        cat_key_pressed(&cat, keys[k], T0 + 10);
        struct cat_view views[64];
        const int n = timeline(&cat, T0 + 10, T0 + 1500, views, NULL, 64);
        for (int i = 0; i < n - 1; i++) {
            CHECK(views[i].valid_ms >= 40);
        }
    }
}

int main(void) {
    test_classify();
    test_walk();
    test_gait();
    test_moonwalk();
    test_arrows();
    test_space_hops();
    test_home_end();
    test_wrap();
    test_keypad_pose();
    test_system_pose();
    test_gaming_pose();
    test_locked_pose();
    test_profile_dash();
    test_away();
    test_idle();
    test_steps_last_a_frame();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("cat_test: all checks passed\n");
    return 0;
}
