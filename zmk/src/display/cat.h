/*
 * The status screen's cat: where it stands and which frame it shows. Pure logic; the
 * glue in status_screen.c feeds it key presses and time. docs/firmware.md describes
 * how the cat behaves.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "cat_frames.h"

/** Pixels that the cat walks per key press. */
#define CAT_STEP_PX 2

/** Positions along the cat's track; the cat wraps from the last one to 0. */
#define CAT_TRACK_LENGTH 41

/** Idle time after the last key press at which the cat sits down. */
#define CAT_SIT_MS 2000

/** Idle time after the last key press at which the cat curls up to sleep. */
#define CAT_SLEEP_MS 30000

/** While sitting, the cat blinks once per period, for CAT_BLINK_MS at its end. */
#define CAT_BLINK_PERIOD_MS 4000
#define CAT_BLINK_MS 150

/** How long each of the two sleeping frames lasts. */
#define CAT_SNORE_MS 1000

struct cat {
    /** Position along the track, 0 to CAT_TRACK_LENGTH - 1, in pixels. */
    int position;
    /** Key presses walked so far; selects the walk frame. */
    uint32_t strides;
};

/** Walks the cat by one step per key press, wrapping at the end of the track. */
void cat_walk(struct cat *cat, uint32_t presses);

/**
 * Returns the frame that the cat shows idle_ms after the last key press, and stores in
 * *valid_ms how long that frame stays correct if no key is pressed.
 */
enum cat_frame cat_frame_at(const struct cat *cat, uint32_t idle_ms, uint32_t *valid_ms);
