/*
 * Shared between the central's PDA sources.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clock.h"

/** Sends this half's info and the clock to the peripheral, if the link is up. Any thread. */
void pda_central_send_state(void);

/**
 * Queues a message of the split data channel (link.h) for the peripheral; it is dropped, with
 * a warning, if the link is down. Any thread.
 */
void pda_central_send(const uint8_t *data, uint8_t len);

/**
 * Asks for split connection latency 0 while on is true, as PDA focus does, so that messages
 * reach the peripheral at the next connection event. Any thread.
 */
void pda_central_hold_fast(bool on);

/**
 * The clock as host_time.c has it: its status, and while that is PDA_CLOCK_SET, the local
 * time now and the profile whose host gave it. Any thread.
 */
enum pda_clock_status pda_host_time_get(uint32_t *local_seconds, uint8_t *profile);
