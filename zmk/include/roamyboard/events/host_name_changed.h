/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include <zmk/event_manager.h>

/**
 * Raised on the system work queue after the host name state of a BLE profile changes:
 * a read starts, finishes or fails, a name is loaded, or a cleared profile forgets its name.
 * roamyboard_host_name_get() returns the new state.
 */
struct roamyboard_host_name_changed {
    /** The BLE profile index whose state changed. */
    uint8_t profile;
};

ZMK_EVENT_DECLARE(roamyboard_host_name_changed);
