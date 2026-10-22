/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/event_manager.h>

/**
 * Raised on the system work queue after the PDA mode changes, on either half, on the
 * peripheral after the PDA's view changes, and on the central after the cat goes to or returns
 * from the peripheral. roamyboard_pda_mode() and roamyboard_pda_cat_away() return the new
 * state.
 */
struct roamyboard_pda_changed {
    char unused;
};

ZMK_EVENT_DECLARE(roamyboard_pda_changed);
