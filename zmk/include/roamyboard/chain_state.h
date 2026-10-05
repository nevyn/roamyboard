/*
 * Key module count of this MCU module's chain, as the kscan driver has accepted it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** Accepted state of a chain whose reads have had no sentinel for stable-scans scans: a fault. */
#define ROAMYBOARD_CHAIN_FAULT (-1)

/** State before the driver has accepted any key module count or fault, shortly after boot. */
#define ROAMYBOARD_CHAIN_UNKNOWN (-2)

/**
 * Returns the accepted state of this MCU module's chain: the key module count (0 or more),
 * ROAMYBOARD_CHAIN_FAULT or ROAMYBOARD_CHAIN_UNKNOWN. Callable from any thread. On a split,
 * each half reports its own chain. roamyboard_chain_state_changed announces every change.
 */
int roamyboard_chain_key_module_count(void);
