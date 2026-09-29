/*
 * Names of the Bluetooth hosts of the BLE profiles, as the hosts report them in their GAP
 * Device Name characteristic (0x2A00). docs/firmware.md describes when they are read.
 *
 * Stored in Zephyr settings, one key per profile:
 *   key    roamyboard/host/<n>, where <n> is the profile index in decimal, 0 to
 *          ZMK_BLE_PROFILE_COUNT - 1 (0 to 4 on the roamyboard)
 *   value  the name as UTF-8 bytes, 1 to ROAMYBOARD_HOST_NAME_MAX bytes, without a
 *          terminating NUL
 * The firmware loads the keys at boot and ignores, with a warning, a key whose index or
 * length is out of range. It writes a key after each read that returns a non-empty name,
 * and deletes it when the profile's pairing is cleared. A name that the host reports as
 * empty is shown but not stored, since settings treat an empty value as a deletion.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** Longest stored name in bytes. Longer names are cut at a UTF-8 character boundary. */
#define ROAMYBOARD_HOST_NAME_MAX 64

enum roamyboard_host_name_state {
    /** No name is known and no read is pending. */
    ROAMYBOARD_HOST_NAME_NONE,
    /** A read of the connected host's name has been sent and not yet answered. */
    ROAMYBOARD_HOST_NAME_PENDING,
    /** name holds the latest name read or stored; it may be empty. */
    ROAMYBOARD_HOST_NAME_KNOWN,
    /** The read could not be sent: error holds bt_gatt_read()'s negative errno. */
    ROAMYBOARD_HOST_NAME_READ_ERROR,
    /** The host answered with, or the stack reported, an ATT error: error holds its code. */
    ROAMYBOARD_HOST_NAME_ATT_ERROR,
};

struct roamyboard_host_name {
    enum roamyboard_host_name_state state;
    /** Negative errno or ATT error code, as state says; 0 otherwise. */
    int error;
    /** NUL-terminated UTF-8, without trailing NULs or whitespace; empty unless state is KNOWN. */
    char name[ROAMYBOARD_HOST_NAME_MAX + 1];
};

/**
 * Copies the host name state of a BLE profile into *out. Callable from any thread.
 *
 * @param profile BLE profile index, 0 to ZMK_BLE_PROFILE_COUNT - 1.
 * @return 0, or -EINVAL if profile is out of range (then *out is state NONE).
 */
int roamyboard_host_name_get(uint8_t profile, struct roamyboard_host_name *out);
