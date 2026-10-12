/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/event_manager.h>

/**
 * Raised on the system work queue after the PDA mode changes, on either half, and on the
 * peripheral after the note's text changes. roamyboard_pda_mode() and
 * roamyboard_pda_note_copy() return the new state.
 */
struct roamyboard_pda_changed {
    char unused;
};

ZMK_EVENT_DECLARE(roamyboard_pda_changed);
