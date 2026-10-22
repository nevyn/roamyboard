/*
 * The micro PDA on the split peripheral's display. While PDA mode is on, the central sends
 * its keyboard keys to the PDA instead of the host. docs/pda.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** The PDA's framebuffer: the whole nice!view, upright, one bit per pixel. */
#define ROAMYBOARD_PDA_FB_WIDTH 68
#define ROAMYBOARD_PDA_FB_HEIGHT 160
#define ROAMYBOARD_PDA_FB_STRIDE ((ROAMYBOARD_PDA_FB_WIDTH + 7) / 8)
#define ROAMYBOARD_PDA_FB_BYTES (ROAMYBOARD_PDA_FB_STRIDE * ROAMYBOARD_PDA_FB_HEIGHT)

enum roamyboard_pda_mode {
    /** Keys go to the host. */
    ROAMYBOARD_PDA_OFF,
    /** Slot 6: the PDA as a sixth device, selected like a Bluetooth profile. */
    ROAMYBOARD_PDA_HOME,
    /** Quick note: the note editor, left with Escape back to the host. */
    ROAMYBOARD_PDA_NOTE,
};

/** The current mode, on either half. Callable from any thread. */
enum roamyboard_pda_mode roamyboard_pda_mode(void);

/**
 * On the central: whether the cat is over on the peripheral's PDA, in the Cat app, so that the
 * status screen shows none. False while the peripheral is not connected. Callable from any
 * thread; roamyboard_pda_changed follows every change.
 */
bool roamyboard_pda_cat_away(void);

/**
 * Central only. Enters a PDA mode, or leaves PDA mode if mode is the current one, so that the
 * same key toggles it. Entering releases every key that the host sees held.
 *
 * @return 0 on success; -ENOTCONN if the peripheral's PDA service is not connected and
 *         subscribed; -ENOTSUP if the firmware's listener order would let keys reach the
 *         host (docs/pda.md, Key capture); -EINVAL for an unknown mode.
 */
int roamyboard_pda_select(enum roamyboard_pda_mode mode);

/**
 * Peripheral only. Draws the PDA's current view into bits: ROAMYBOARD_PDA_FB_HEIGHT rows of
 * ROAMYBOARD_PDA_FB_STRIDE bytes, MSB leftmost, a set bit lit (the PDA draws light on dark).
 *
 * Call it on the display work queue only: apps draw text with LVGL.
 */
void roamyboard_pda_render(uint8_t bits[ROAMYBOARD_PDA_FB_BYTES]);
