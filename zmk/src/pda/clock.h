/*
 * The PDA's clock: local time from a host's Current Time Service, as seconds since
 * 2000-01-01 00:00 in the host's time zone. Pure C, so that zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Where the clock stands, as the central reports it. */
enum pda_clock_status {
    /** No host has been asked yet. */
    PDA_CLOCK_UNKNOWN,
    /** A host answered; the time is valid. */
    PDA_CLOCK_SET,
    /** No connected host has the Current Time characteristic. */
    PDA_CLOCK_NO_CTS,
    /** A read failed, or a host sent a malformed time. */
    PDA_CLOCK_ERROR,
};

struct pda_date_time {
    int year, month, day, hour, minute, second;
    /** 1 for Monday to 7 for Sunday. */
    int weekday;
};

/**
 * Parses the Current Time characteristic (0x2A2B): year (little-endian), month, day, hours,
 * minutes and seconds, then fields that the clock ignores.
 *
 * @return True and *seconds since 2000-01-01 00:00 local time, or false if value is shorter
 *         than 7 bytes or holds a date before 2000, after 2135, or out of range.
 */
bool pda_clock_from_cts(const uint8_t *value, size_t len, uint32_t *seconds);

/** Splits seconds since 2000-01-01 00:00 into a date and time. */
struct pda_date_time pda_clock_split(uint32_t seconds);
