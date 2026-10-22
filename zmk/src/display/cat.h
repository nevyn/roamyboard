/*
 * The status screen's cat: where it is, which way it faces and which frame it shows. Pure
 * logic; the glue in status_screen.c feeds it key presses, layer changes and time.
 * docs/firmware.md describes how the cat behaves.
 *
 * All times are k_uptime_get_32() milliseconds; they may wrap. Events must come in time
 * order, and no event or query may be more than 2^31 ms after the one before it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cat_frames.h"

/** Width of the key module count box's interior, which the cat walks across, in pixels. */
#define CAT_BOX_WIDTH 66

/** Positions per lap: the cat's x wraps from fully off the right edge to fully off the left. */
#define CAT_LAP_PX (CAT_BOX_WIDTH + CAT_WIDTH)

/** Highest that the cat rises above its floor, in pixels; higher would touch the count text. */
#define CAT_MAX_RISE_PX 5

/** Pixels that the cat walks per typing key, and runs per typing key while running. */
#define CAT_STEP_PX 2
#define CAT_RUN_STEP_PX 4

/** Typing keys closer together than this make the cat run instead of walk. */
#define CAT_RUN_INTERVAL_MS 150

/** While scampering or dashing, the cat moves CAT_DASH_STEP_PX every CAT_DASH_STEP_MS. */
#define CAT_DASH_STEP_PX 6
#define CAT_DASH_STEP_MS 40

/** While a mouse-move key is held, the cat stalks CAT_STEP_PX every CAT_STALK_STEP_MS. */
#define CAT_STALK_STEP_MS 200

/** On a layer with a pose, the cat takes the pose this long after its last stride. */
#define CAT_SETTLE_MS 400

/** In the Gaming pose the rump wiggles, one shift per step, for CAT_WIGGLE_MS, then stays still. */
#define CAT_WIGGLE_MS 1200
#define CAT_WIGGLE_STEP_MS 150

/** Idle time after the cat came to rest at which it sits down (with no layer pose). */
#define CAT_SIT_MS 2000

/** Idle time after the cat came to rest at which it curls up to sleep, in every pose. */
#define CAT_SLEEP_MS 30000

/** While sitting, the cat blinks once per period, for CAT_BLINK_MS at its end. */
#define CAT_BLINK_PERIOD_MS 4000
#define CAT_BLINK_MS 150

/** How long each of the two sleeping frames lasts. */
#define CAT_SNORE_MS 1000

/** What a key means to the cat; cat_classify_key() derives it from the HID usage. */
enum cat_key {
    /** Not a typing key: modifiers, F-keys, Esc, Caps Lock, media keys and the like. */
    CAT_KEY_OTHER,
    /** Letters, digits, punctuation, Tab, and the keypad's digits, operators and dot. */
    CAT_KEY_TYPING,
    CAT_KEY_SPACE,
    /** Enter and keypad Enter. */
    CAT_KEY_ENTER,
    /** Backspace and Delete. */
    CAT_KEY_BACKSPACE,
    CAT_KEY_LEFT,
    CAT_KEY_RIGHT,
    CAT_KEY_UP,
    CAT_KEY_DOWN,
    CAT_KEY_HOME,
    CAT_KEY_END,
};

/** The cat's resting pose, chosen by the highest active layer. */
enum cat_pose {
    /** The base layer, or a layer without a pose: stands, sits after a while, sleeps. */
    CAT_POSE_NONE,
    /** Sits up with its ears perked. */
    CAT_POSE_KEYPAD,
    /** Hunting crouch; mouse keys make it stalk and pounce. */
    CAT_POSE_SYSTEM,
    /** Ready to pounce; every key press is a leap. */
    CAT_POSE_GAMING,
    /** A loaf in the middle of the box, ignoring every key. */
    CAT_POSE_LOCKED,
};

/** A mouse key: a mouse-move direction or a button. */
enum cat_mouse {
    CAT_MOUSE_LEFT,
    CAT_MOUSE_RIGHT,
    CAT_MOUSE_UP,
    CAT_MOUSE_DOWN,
    CAT_MOUSE_CLICK,
};

/** What the screen shows of the cat at one moment. */
struct cat_view {
    /** The frame, CAT_FACING_LEFT + frame when the cat faces left. */
    enum cat_frame frame;
    /**
     * Left edge of the frame from the left edge of the box interior, in pixels:
     * -CAT_WIDTH (just off the left edge) to CAT_BOX_WIDTH - 1 (one pixel column visible
     * at the right edge). The screen crops whatever lies outside the interior.
     */
    int x;
    /** Height above the floor, 0 to CAT_MAX_RISE_PX. */
    int y;
    /** How long, in ms and at least 1, this view stays correct if no event comes. */
    uint32_t valid_ms;
};

/** One timed step of a scripted action. */
struct cat_step {
    /** How long the step lasts. */
    uint16_t ms;
    /** The frame, facing right; the action's facing applies on top. */
    uint8_t frame;
    /** Distance moved since the script began, in the direction the cat faces. */
    int8_t dx;
    /** Height above the floor. */
    int8_t dy;
};

/** The cat's state. Initialize with cat_init(); the fields are private to cat.c. */
struct cat {
    /** Where the cat ends up once its action ends, wrapped like cat_view.x. */
    int x;
    bool left;
    /** Strides so far, minus moonwalk strides; selects the walk and run frames. */
    int32_t strides;
    enum cat_pose pose;
    uint32_t pose_ms;
    /** When the cat came, or comes, to rest: idle time counts from here. */
    uint32_t rest_ms;
    /** Frame held after a stride, until the pose takes over; CAT_FRAME_COUNT if none. */
    enum cat_frame rest_frame;
    bool typed;
    uint32_t typed_ms;

    /** The running action: first a run of run_px from from_x, then the script. */
    const struct cat_step *script;
    uint8_t script_len;
    uint32_t action_ms;
    int from_x;
    int run_px;
    bool action_left;

    /** A held mouse-move key, and where and when stalking began. */
    bool stalking;
    enum cat_mouse stalk;
    uint32_t stalk_ms;
    int stalk_x;

    /** The cat is on the other half, in the PDA's Cat app, once its dash off the edge ends. */
    bool away;
};

/**
 * Returns what a key means to the cat.
 *
 * @param usage_page HID usage page; only the keyboard page (0x07) has cat keys.
 * @param keycode HID usage ID on that page, without modifiers.
 */
enum cat_key cat_classify_key(uint16_t usage_page, uint32_t keycode);

/** Puts the cat at rest at x (see cat_view.x), facing right, with no pose, at now_ms. */
void cat_init(struct cat *cat, int x, uint32_t now_ms);

/**
 * Feeds a key press. Typing keys walk the cat right, or run it when they come fast; Space
 * also hops, Enter and Up jump, Backspace moonwalks left, Left and Right turn and walk,
 * Down crouches, Home and End scamper to the box's ends. In the Gaming pose every key,
 * CAT_KEY_OTHER included, is a leap; in the Locked pose keys do nothing.
 */
void cat_key_pressed(struct cat *cat, enum cat_key key, uint32_t now_ms);

/** Feeds a mouse key press: a move key stalks while held, a click pounces. */
void cat_mouse_pressed(struct cat *cat, enum cat_mouse mouse, uint32_t now_ms);

/** Feeds a mouse key release; releasing the held move key ends the stalk. */
void cat_mouse_released(struct cat *cat, enum cat_mouse mouse, uint32_t now_ms);

/**
 * Sets the pose. Entering the Locked pose sends the cat to the middle of the box to flop
 * down; leaving it makes the cat stretch. Setting the current pose again does nothing.
 */
void cat_set_pose(struct cat *cat, enum cat_pose pose, uint32_t now_ms);

/** Makes the cat dash off the right edge and come back in from the left to where it was. */
void cat_profile_changed(struct cat *cat, uint32_t now_ms);

/**
 * Sends the cat to the other half's PDA or brings it back. Going, it dashes off the right
 * edge and stays away: keys and mouse keys do nothing until it returns, and cat_view_at()
 * shows no frame. Returning, it runs in from the right edge to CAT_RETURN_X and faces left.
 * Setting the current state again does nothing.
 */
void cat_set_away(struct cat *cat, bool away, uint32_t now_ms);

/** Where the cat stops after returning from the other half. */
#define CAT_RETURN_X (CAT_BOX_WIDTH - CAT_WIDTH - 8)

/**
 * Returns what the screen shows of the cat at now_ms, which must not precede the last event;
 * the frame is CAT_FRAME_COUNT while the cat is away.
 */
struct cat_view cat_view_at(const struct cat *cat, uint32_t now_ms);
