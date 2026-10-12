/*
 * Shared between the central's PDA sources.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "clock.h"

/** Sends this half's info and the clock to the peripheral, if the link is up. Any thread. */
void pda_central_send_state(void);

/**
 * The clock as host_time.c has it: its status, and while that is PDA_CLOCK_SET, the local
 * time now and the profile whose host gave it. Any thread.
 */
enum pda_clock_status pda_host_time_get(uint32_t *local_seconds, uint8_t *profile);
