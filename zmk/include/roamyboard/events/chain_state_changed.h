/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/event_manager.h>

/**
 * Raised by the kscan driver, on the system work queue, whenever it accepts a new state of
 * this MCU module's chain: a new key module count, or a fault. Each half of a split raises
 * it for its own chain only; the central never learns the peripheral's count.
 */
struct roamyboard_chain_state_changed {
    /** The new accepted key module count (0 or more), or ROAMYBOARD_CHAIN_FAULT. */
    int key_module_count;
};

ZMK_EVENT_DECLARE(roamyboard_chain_state_changed);
