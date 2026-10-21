/*
 * Host tests for zmk/drivers/input/iqs5xx_logic.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "iqs5xx_logic.h"

#include <stdio.h>
#include <string.h>

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

#define CHECK_EVENT(out, i, kind_, button_, value_, sync_)                                         \
    do {                                                                                           \
        CHECK((size_t)(i) < (out).count);                                                          \
        if ((size_t)(i) < (out).count) {                                                           \
            const struct iqs5xx_event *e_ = &(out).events[i];                                      \
            CHECK_EQ(e_->kind, kind_);                                                             \
            CHECK_EQ(e_->button, button_);                                                         \
            CHECK_EQ(e_->value, value_);                                                           \
            CHECK_EQ(e_->sync, sync_);                                                             \
        }                                                                                          \
    } while (0)

static struct iqs5xx_frame frame(uint8_t g0, uint8_t g1, uint8_t fingers, int16_t x, int16_t y) {
    return (struct iqs5xx_frame){
        .gestures0 = g0,
        .gestures1 = g1,
        .fingers = fingers,
        .rel_x = x,
        .rel_y = y,
    };
}

static void run(struct iqs5xx_logic *logic, struct iqs5xx_frame f, struct iqs5xx_output *out) {
    iqs5xx_logic_frame(logic, &f, out);
}

static void test_decode(void) {
    const uint8_t raw[IQS5XX_FRAME_LEN] = {0x21, 0x02, 0x83, 0x05, 0x02, 0xFF, 0xFE, 0x01, 0x2C};
    struct iqs5xx_frame f;
    iqs5xx_frame_decode(raw, &f);
    CHECK_EQ(f.gestures0, 0x21);
    CHECK_EQ(f.gestures1, IQS5XX_SCROLL);
    CHECK(f.system_info0 & IQS5XX_SHOW_RESET);
    CHECK_EQ(f.system_info0 & IQS5XX_CHARGING_MODE_MASK, 3);
    CHECK(f.system_info1 & IQS5XX_TP_MOVEMENT);
    CHECK(f.system_info1 & IQS5XX_TOO_MANY_FINGERS);
    CHECK_EQ(f.fingers, 2);
    CHECK_EQ(f.rel_x, -2);
    CHECK_EQ(f.rel_y, 300);

    const uint8_t extremes[IQS5XX_FRAME_LEN] = {0, 0, 0, 0, 1, 0x80, 0x00, 0x7F, 0xFF};
    iqs5xx_frame_decode(extremes, &f);
    CHECK_EQ(f.rel_x, -32768);
    CHECK_EQ(f.rel_y, 32767);
}

static void test_one_finger_moves(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(0, 0, 1, 5, -7), &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_REL_X, 0, 5, false);
    CHECK_EVENT(out, 1, IQS5XX_EV_REL_Y, 0, -7, true);
    CHECK(!out.tap_release_due);

    run(&logic, frame(0, 0, 1, 0, -3), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_REL_Y, 0, -3, true);

    run(&logic, frame(0, 0, 1, -4, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_REL_X, 0, -4, true);

    run(&logic, frame(0, 0, 1, 0, 0), &out);
    CHECK_EQ(out.count, 0);
}

static void test_movement_needs_one_finger_and_no_gesture(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(0, 0, 2, 5, 5), &out);
    CHECK_EQ(out.count, 0);
    run(&logic, frame(0, 0, 0, 5, 5), &out);
    CHECK_EQ(out.count, 0);

    const uint8_t swipes[] = {IQS5XX_SWIPE_X_NEG, IQS5XX_SWIPE_X_POS, IQS5XX_SWIPE_Y_POS,
                              IQS5XX_SWIPE_Y_NEG};
    for (size_t i = 0; i < sizeof(swipes); i++) {
        run(&logic, frame(swipes[i], 0, 1, 40, 0), &out);
        CHECK_EQ(out.count, 0);
    }
    run(&logic, frame(0, IQS5XX_ZOOM, 2, 10, 10), &out);
    CHECK_EQ(out.count, 0);
    run(&logic, frame(0, IQS5XX_ZOOM, 1, 10, 10), &out);
    CHECK_EQ(out.count, 0);
}

static void test_single_tap_clicks_button_0(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 0, 0, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 1, true);
    CHECK(out.tap_release_due);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, true);
    CHECK(!out.tap_release_due);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 0);
}

static void test_two_finger_tap_clicks_button_1(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(0, IQS5XX_TWO_FINGER_TAP, 0, 0, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 1, 1, true);
    CHECK(out.tap_release_due);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 1, 0, true);
}

static void test_tap_while_tap_pending_clicks_twice(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 0, 0, 0), &out);
    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 0, 0, 0), &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, true);
    CHECK_EVENT(out, 1, IQS5XX_EV_BUTTON, 0, 1, true);
    CHECK(out.tap_release_due);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, true);
}

static void test_both_taps_release_together(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 0, 0, 0), &out);
    run(&logic, frame(0, IQS5XX_TWO_FINGER_TAP, 0, 0, 0), &out);
    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, false);
    CHECK_EVENT(out, 1, IQS5XX_EV_BUTTON, 1, 0, true);
}

static void test_press_and_hold_drags_until_lift(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_PRESS_AND_HOLD, 0, 1, 0, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 1, true);
    CHECK(!out.tap_release_due);

    run(&logic, frame(IQS5XX_PRESS_AND_HOLD, 0, 1, 3, -2), &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_REL_X, 0, 3, false);
    CHECK_EVENT(out, 1, IQS5XX_EV_REL_Y, 0, -2, true);

    run(&logic, frame(0, 0, 1, -1, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_REL_X, 0, -1, true);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 0);

    run(&logic, frame(0, 0, 0, 0, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, true);

    run(&logic, frame(0, 0, 0, 0, 0), &out);
    CHECK_EQ(out.count, 0);
}

static void test_hold_ignores_a_tap_on_its_button(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_PRESS_AND_HOLD, 0, 1, 0, 0), &out);
    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 1, 0, 0), &out);
    CHECK_EQ(out.count, 0);
    CHECK(!out.tap_release_due);
}

static void test_hold_after_tap_releases_the_tap_first(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(IQS5XX_SINGLE_TAP, 0, 0, 0, 0), &out);
    run(&logic, frame(IQS5XX_PRESS_AND_HOLD, 0, 1, 0, 0), &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, true);
    CHECK_EVENT(out, 1, IQS5XX_EV_BUTTON, 0, 1, true);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 0);
}

static void test_scroll_keeps_the_remainder(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, 20), &out);
    CHECK_EQ(out.count, 0);
    CHECK_EQ(logic.scroll_y, 20);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, 20), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_WHEEL, 0, 1, true);
    CHECK_EQ(logic.scroll_y, 8);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, 60), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_WHEEL, 0, 2, true);
    CHECK_EQ(logic.scroll_y, 4);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, -10), &out);
    CHECK_EQ(out.count, 0);
    CHECK_EQ(logic.scroll_y, -6);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, -60), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_WHEEL, 0, -2, true);
    CHECK_EQ(logic.scroll_y, -2);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, -31, 0), &out);
    CHECK_EQ(out.count, 0);
    CHECK_EQ(logic.scroll_x, -31);
    run(&logic, frame(0, IQS5XX_SCROLL, 2, -1, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_HWHEEL, 0, -1, true);
    CHECK_EQ(logic.scroll_x, 0);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 70, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_HWHEEL, 0, 2, true);
    CHECK_EQ(logic.scroll_x, 6);
}

static void test_scroll_emits_no_movement(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 1);

    run(&logic, frame(0, IQS5XX_SCROLL, 1, 3, 0), &out);
    CHECK_EQ(out.count, 1);
    CHECK_EVENT(out, 0, IQS5XX_EV_HWHEEL, 0, 3, true);
}

static void test_lift_drops_the_scroll_remainder(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 20, -20), &out);
    run(&logic, frame(0, 0, 2, 0, 0), &out);
    CHECK_EQ(logic.scroll_x, 20);
    CHECK_EQ(logic.scroll_y, -20);

    run(&logic, frame(0, 0, 0, 0, 0), &out);
    CHECK_EQ(out.count, 0);
    CHECK_EQ(logic.scroll_x, 0);
    CHECK_EQ(logic.scroll_y, 0);

    run(&logic, frame(0, IQS5XX_SCROLL, 2, 20, -20), &out);
    CHECK_EQ(out.count, 0);
}

static void test_release_all(void) {
    struct iqs5xx_logic logic;
    struct iqs5xx_output out;
    iqs5xx_logic_init(&logic, 32);

    iqs5xx_logic_release_all(&logic, &out);
    CHECK_EQ(out.count, 0);

    run(&logic, frame(IQS5XX_PRESS_AND_HOLD, 0, 1, 0, 0), &out);
    run(&logic, frame(0, IQS5XX_TWO_FINGER_TAP, 2, 0, 0), &out);
    run(&logic, frame(0, IQS5XX_SCROLL, 2, 0, 10), &out);
    iqs5xx_logic_release_all(&logic, &out);
    CHECK_EQ(out.count, 2);
    CHECK_EVENT(out, 0, IQS5XX_EV_BUTTON, 0, 0, false);
    CHECK_EVENT(out, 1, IQS5XX_EV_BUTTON, 1, 0, true);
    CHECK_EQ(logic.scroll_y, 0);
    CHECK(!logic.holding);
    CHECK_EQ(logic.tapped, 0);

    iqs5xx_logic_release_taps(&logic, &out);
    CHECK_EQ(out.count, 0);
}

static void test_products(void) {
    CHECK(iqs5xx_product_known(58));
    CHECK(iqs5xx_product_known(40));
    CHECK(iqs5xx_product_known(52));
    CHECK(!iqs5xx_product_known(0));
    CHECK(!iqs5xx_product_known(0xFFFF));
}

#define CHECK_TRANSITION(state, input, product, next_, actions_)                                   \
    do {                                                                                           \
        const struct iqs5xx_transition t_ = iqs5xx_next(state, input, product);                    \
        CHECK_EQ(t_.next, next_);                                                                  \
        CHECK_EQ(t_.actions, actions_);                                                            \
    } while (0)

static void test_transitions_absent(void) {
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_PROBE_OK, 58, IQS5XX_CONFIGURING,
                     IQS5XX_DO_CONFIGURE);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_PROBE_OK, 40, IQS5XX_CONFIGURING,
                     IQS5XX_DO_CONFIGURE);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_PROBE_OK, 7, IQS5XX_ABSENT,
                     IQS5XX_DO_WARN_PRODUCT | IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_PROBE_FAILED, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_IO_ERROR, 0, IQS5XX_ABSENT, IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_SHOW_RESET, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_ABSENT, IQS5XX_IN_CONFIGURED, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_PROBE_LATER);
}

static void test_transitions_configuring(void) {
    CHECK_TRANSITION(IQS5XX_CONFIGURING, IQS5XX_IN_CONFIGURED, 0, IQS5XX_RUNNING,
                     IQS5XX_DO_ENABLE_RDY | IQS5XX_DO_CHECK_LATER);
    CHECK_TRANSITION(IQS5XX_CONFIGURING, IQS5XX_IN_SHOW_RESET, 0, IQS5XX_CONFIGURING,
                     IQS5XX_DO_CONFIGURE);
    CHECK_TRANSITION(IQS5XX_CONFIGURING, IQS5XX_IN_PROBE_OK, 58, IQS5XX_CONFIGURING,
                     IQS5XX_DO_CONFIGURE);
    CHECK_TRANSITION(IQS5XX_CONFIGURING, IQS5XX_IN_IO_ERROR, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_CONFIGURING, IQS5XX_IN_PROBE_FAILED, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_PROBE_LATER);
}

static void test_transitions_running(void) {
    CHECK_TRANSITION(IQS5XX_RUNNING, IQS5XX_IN_SHOW_RESET, 0, IQS5XX_CONFIGURING,
                     IQS5XX_DO_DISABLE_RDY | IQS5XX_DO_RELEASE | IQS5XX_DO_CONFIGURE);
    CHECK_TRANSITION(IQS5XX_RUNNING, IQS5XX_IN_IO_ERROR, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_DISABLE_RDY | IQS5XX_DO_RELEASE | IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_RUNNING, IQS5XX_IN_PROBE_FAILED, 0, IQS5XX_ABSENT,
                     IQS5XX_DO_DISABLE_RDY | IQS5XX_DO_RELEASE | IQS5XX_DO_PROBE_LATER);
    CHECK_TRANSITION(IQS5XX_RUNNING, IQS5XX_IN_PROBE_OK, 58, IQS5XX_RUNNING,
                     IQS5XX_DO_CHECK_LATER);
    CHECK_TRANSITION(IQS5XX_RUNNING, IQS5XX_IN_CONFIGURED, 0, IQS5XX_RUNNING,
                     IQS5XX_DO_CHECK_LATER);
}

static void test_state_names(void) {
    CHECK(strcmp(iqs5xx_state_name(IQS5XX_ABSENT), "absent") == 0);
    CHECK(strcmp(iqs5xx_state_name(IQS5XX_CONFIGURING), "configuring") == 0);
    CHECK(strcmp(iqs5xx_state_name(IQS5XX_RUNNING), "running") == 0);
}

int main(void) {
    test_decode();
    test_one_finger_moves();
    test_movement_needs_one_finger_and_no_gesture();
    test_single_tap_clicks_button_0();
    test_two_finger_tap_clicks_button_1();
    test_tap_while_tap_pending_clicks_twice();
    test_both_taps_release_together();
    test_press_and_hold_drags_until_lift();
    test_hold_ignores_a_tap_on_its_button();
    test_hold_after_tap_releases_the_tap_first();
    test_scroll_keeps_the_remainder();
    test_scroll_emits_no_movement();
    test_lift_drops_the_scroll_remainder();
    test_release_all();
    test_products();
    test_transitions_absent();
    test_transitions_configuring();
    test_transitions_running();
    test_state_names();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("iqs5xx_test: all checks passed\n");
    return 0;
}
