/*
 * The micro PDA on the split peripheral's display. While PDA mode is on, the central sends
 * its keyboard keys to the PDA instead of the host. docs/pda.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>

/** The longest note in bytes of UTF-8, without the terminating NUL. */
#define ROAMYBOARD_PDA_NOTE_MAX 512

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
 * Central only. Enters a PDA mode, or leaves PDA mode if mode is the current one, so that the
 * same key toggles it. Entering releases every key that the host sees held.
 *
 * @return 0 on success; -ENOTCONN if the peripheral's PDA service is not connected and
 *         subscribed; -ENOTSUP if the firmware's listener order would let keys reach the
 *         host (docs/pda.md, Key capture); -EINVAL for an unknown mode.
 */
int roamyboard_pda_select(enum roamyboard_pda_mode mode);

/**
 * Peripheral only. Copies the note's UTF-8 text, NUL-terminated and cut to size - 1 bytes.
 *
 * @return The text's length in bytes before the cut.
 */
size_t roamyboard_pda_note_copy(char *buf, size_t size);
