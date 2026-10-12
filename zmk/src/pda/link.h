/*
 * The split data channel between the halves: a GATT service on the split peripheral that the
 * central writes to and subscribes to, over the bonded split connection. docs/pda.md describes
 * the messages.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

#define PDA_LINK_UUID(num) BT_UUID_128_ENCODE(num, 0xe6f3, 0x4aec, 0xaa09, 0x6d2205e560e3)
#define PDA_LINK_SERVICE_UUID PDA_LINK_UUID(0xb639bb63)
/** Central to peripheral: write without response. */
#define PDA_LINK_TO_PERIPHERAL_UUID PDA_LINK_UUID(0xb639bb64)
/** Peripheral to central: notify. */
#define PDA_LINK_TO_CENTRAL_UUID PDA_LINK_UUID(0xb639bb65)

/** First byte of every message. */
enum pda_link_op {
    /** Central to peripheral: [op, enum roamyboard_pda_mode]. */
    PDA_LINK_MODE = 0x01,
    /** Central to peripheral: [op, pressed, implicit modifier bits, HID keyboard usage ID]. */
    PDA_LINK_KEY = 0x02,
    /** Central to peripheral, in PDA mode: [op, pressed, key position]. */
    PDA_LINK_POSITION = 0x03,
    /**
     * Central to peripheral: [op, key module count as int8, battery percent, build ID...];
     * the central's own state, for the Info app. Sent when the link comes up and on change.
     */
    PDA_LINK_INFO = 0x04,
    /**
     * Central to peripheral: [op, enum pda_clock_status, profile, local seconds since
     * 2000-01-01 as uint32 little-endian]; sent with PDA_LINK_INFO.
     */
    PDA_LINK_TIME = 0x05,
    /** Peripheral to central: [op, UTF-8 bytes...]; appends to the text to type out. */
    PDA_LINK_TEXT = 0x81,
    /** Peripheral to central: [op]; leave PDA mode, then type the text out to the host. */
    PDA_LINK_TYPE_OUT = 0x82,
    /** Peripheral to central: [op]; leave PDA mode. */
    PDA_LINK_LEAVE = 0x83,
};

#define PDA_LINK_KEY_LEN 4
#define PDA_LINK_POSITION_LEN 3
#define PDA_LINK_TIME_LEN 7
/** The longest message from the central; writes stay within the smallest ATT MTU (23). */
#define PDA_LINK_WRITE_MAX 20
