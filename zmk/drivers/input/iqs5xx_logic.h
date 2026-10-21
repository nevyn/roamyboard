/*
 * Pure logic of the IQS5xx-B000 trackpad driver: decoding the frame that the
 * driver reads in each communication window, turning frames into pointer
 * events, and the driver's state machine. No Zephyr dependencies, so the host
 * tests in zmk/tests compile it with cc. docs/firmware.md describes the driver.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** First register of the frame: Gesture Events 0. */
#define IQS5XX_FRAME_START 0x000D
/** Bytes from Gesture Events 0 (0x000D) to the low byte of Relative Y (0x0015). */
#define IQS5XX_FRAME_LEN 9

/** Gesture Events 0 (0x000D) and the single finger gesture enables (0x06B7). */
#define IQS5XX_SINGLE_TAP 0x01
#define IQS5XX_PRESS_AND_HOLD 0x02
#define IQS5XX_SWIPE_X_NEG 0x04
#define IQS5XX_SWIPE_X_POS 0x08
#define IQS5XX_SWIPE_Y_POS 0x10
#define IQS5XX_SWIPE_Y_NEG 0x20

/** Gesture Events 1 (0x000E) and the multi finger gesture enables (0x06B8). */
#define IQS5XX_TWO_FINGER_TAP 0x01
#define IQS5XX_SCROLL 0x02
#define IQS5XX_ZOOM 0x04

/** System Info 0 (0x000F). */
#define IQS5XX_SHOW_RESET 0x80
#define IQS5XX_CHARGING_MODE_MASK 0x07

/** System Info 1 (0x0010). */
#define IQS5XX_TP_MOVEMENT 0x01
#define IQS5XX_PALM_DETECT 0x02
#define IQS5XX_TOO_MANY_FINGERS 0x04

/** Mouse buttons that the logic presses: 0 is the primary button, 1 the secondary. */
#define IQS5XX_BUTTONS 2

/** Most events that one call into the logic emits: a frame with every rule firing emits 11. */
#define IQS5XX_MAX_EVENTS 12

/** One communication window's registers 0x000D to 0x0015, decoded. */
struct iqs5xx_frame {
    uint8_t gestures0;
    uint8_t gestures1;
    uint8_t system_info0;
    uint8_t system_info1;
    uint8_t fingers;
    /** Movement since the previous cycle, in trackpad pixels; during a scroll, only the scroll axis. */
    int16_t rel_x;
    int16_t rel_y;
};

enum iqs5xx_event_kind {
    IQS5XX_EV_REL_X,
    IQS5XX_EV_REL_Y,
    /** Vertical wheel steps; the sign follows Relative Y. */
    IQS5XX_EV_WHEEL,
    /** Horizontal wheel steps; the sign follows Relative X. */
    IQS5XX_EV_HWHEEL,
    /** A mouse button: value 1 presses it, 0 releases it. */
    IQS5XX_EV_BUTTON,
};

struct iqs5xx_event {
    enum iqs5xx_event_kind kind;
    /** Button index, 0 to IQS5XX_BUTTONS - 1; only for IQS5XX_EV_BUTTON. */
    uint8_t button;
    int32_t value;
    /** The receiver must act on the events up to and including this one before the next. */
    bool sync;
};

struct iqs5xx_output {
    struct iqs5xx_event events[IQS5XX_MAX_EVENTS];
    size_t count;
    /**
     * A tap pressed a button. The caller must call iqs5xx_logic_release_taps() a short
     * time later, and may postpone a release that is already pending to that time.
     */
    bool tap_release_due;
};

/** State that the logic carries from one frame to the next. */
struct iqs5xx_logic {
    /** Relative counts per wheel step, >= 1. */
    int32_t scroll_divisor;
    /** Scroll counts not yet sent as a wheel step, each between -(divisor - 1) and divisor - 1. */
    int32_t scroll_x;
    int32_t scroll_y;
    /** Button 0 is held by press-and-hold until the fingers lift (a drag). */
    bool holding;
    /** Bit n: button n is pressed by a tap and waits for iqs5xx_logic_release_taps(). */
    uint8_t tapped;
};

/** Decodes the IQS5XX_FRAME_LEN bytes read from IQS5XX_FRAME_START. */
void iqs5xx_frame_decode(const uint8_t raw[IQS5XX_FRAME_LEN], struct iqs5xx_frame *frame);

/** Prepares the logic with no button pressed. scroll_divisor must be >= 1. */
void iqs5xx_logic_init(struct iqs5xx_logic *logic, int32_t scroll_divisor);

/**
 * Turns one frame into pointer events:
 *
 * - The fingers lifting (0 fingers) releases a press-and-hold and drops the scroll remainders.
 * - SINGLE_TAP presses button 0 and TWO_FINGER_TAP button 1, each released by
 *   iqs5xx_logic_release_taps(). A tap on a button that a tap still holds releases it first.
 * - PRESS_AND_HOLD presses button 0 and holds it until the fingers lift.
 * - SCROLL adds Relative X and Y to the scroll remainders and emits a wheel step for every
 *   scroll_divisor counts, keeping the rest.
 * - One finger that moves, with no gesture but press-and-hold, emits Relative X and Y.
 * - Swipes and zoom emit nothing, nor does any movement in their frame.
 *
 * The last event has sync set. output is overwritten.
 */
void iqs5xx_logic_frame(struct iqs5xx_logic *logic, const struct iqs5xx_frame *frame,
                        struct iqs5xx_output *output);

/** Releases the buttons that taps pressed. output is overwritten. */
void iqs5xx_logic_release_taps(struct iqs5xx_logic *logic, struct iqs5xx_output *output);

/**
 * Releases every pressed button and drops the scroll remainders, for a trackpad that went
 * away or reset. output is overwritten.
 */
void iqs5xx_logic_release_all(struct iqs5xx_logic *logic, struct iqs5xx_output *output);

/** Whether a Product number (0x0000) names an IQS5xx-B000: IQS550 (40), IQS572 (58) or IQS525 (52). */
bool iqs5xx_product_known(uint16_t product);

enum iqs5xx_state {
    /** No trackpad answers; the driver probes for one. */
    IQS5XX_ABSENT,
    /** A trackpad answered or reset; the driver writes its configuration. */
    IQS5XX_CONFIGURING,
    /** The trackpad is configured; the driver reads a frame on each RDY rising edge. */
    IQS5XX_RUNNING,
};

enum iqs5xx_input {
    /** A probe read a Product number; iqs5xx_next() takes it as its product argument. */
    IQS5XX_IN_PROBE_OK,
    /** A probe got no answer. */
    IQS5XX_IN_PROBE_FAILED,
    /** A transfer failed outside a probe. */
    IQS5XX_IN_IO_ERROR,
    /** The trackpad reports SHOW_RESET: it reset and lost its configuration. */
    IQS5XX_IN_SHOW_RESET,
    /** The configuration was written and the communication window ended. */
    IQS5XX_IN_CONFIGURED,
};

/** What the driver must do on a transition, as a bit set, in this order. */
enum iqs5xx_action {
    /** Disable the RDY interrupt and cancel a pending frame read. */
    IQS5XX_DO_DISABLE_RDY = 1 << 0,
    /** Call iqs5xx_logic_release_all() and report its events. */
    IQS5XX_DO_RELEASE = 1 << 1,
    /** Write the configuration now, then feed IQS5XX_IN_CONFIGURED or IQS5XX_IN_IO_ERROR. */
    IQS5XX_DO_CONFIGURE = 1 << 2,
    /** Enable the RDY interrupt, and read a frame if RDY is already high. */
    IQS5XX_DO_ENABLE_RDY = 1 << 3,
    /** Schedule the next probe. */
    IQS5XX_DO_PROBE_LATER = 1 << 4,
    /** Schedule the next liveness check. */
    IQS5XX_DO_CHECK_LATER = 1 << 5,
    /** The device at the address is not an IQS5xx-B000: report its product and keep probing. */
    IQS5XX_DO_WARN_PRODUCT = 1 << 6,
};

struct iqs5xx_transition {
    enum iqs5xx_state next;
    /** A bit set of enum iqs5xx_action. */
    unsigned int actions;
};

/**
 * The state machine: which state follows state on input, and what the driver must do.
 *
 * @param product The Product number for IQS5XX_IN_PROBE_OK; ignored otherwise.
 */
struct iqs5xx_transition iqs5xx_next(enum iqs5xx_state state, enum iqs5xx_input input,
                                     uint16_t product);

/** The state's name for logs. */
const char *iqs5xx_state_name(enum iqs5xx_state state);
