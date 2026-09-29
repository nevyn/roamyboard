/*
 * SPDX-License-Identifier: MIT
 */

#include "cat.h"

void cat_walk(struct cat *cat, uint32_t presses) {
    cat->position = (int)((cat->position + (uint64_t)presses * CAT_STEP_PX) % CAT_TRACK_LENGTH);
    cat->strides += presses;
}

static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

enum cat_frame cat_frame_at(const struct cat *cat, uint32_t idle_ms, uint32_t *valid_ms) {
    if (idle_ms < CAT_SIT_MS) {
        *valid_ms = CAT_SIT_MS - idle_ms;
        return (enum cat_frame)(CAT_WALK_0 + cat->strides % 4);
    }

    if (idle_ms < CAT_SLEEP_MS) {
        const uint32_t phase = (idle_ms - CAT_SIT_MS) % CAT_BLINK_PERIOD_MS;
        const uint32_t until_sleep = CAT_SLEEP_MS - idle_ms;
        if (phase < CAT_BLINK_PERIOD_MS - CAT_BLINK_MS) {
            *valid_ms = min_u32(CAT_BLINK_PERIOD_MS - CAT_BLINK_MS - phase, until_sleep);
            return CAT_SIT;
        }
        *valid_ms = min_u32(CAT_BLINK_PERIOD_MS - phase, until_sleep);
        return CAT_BLINK;
    }

    const uint32_t asleep = idle_ms - CAT_SLEEP_MS;
    *valid_ms = CAT_SNORE_MS - asleep % CAT_SNORE_MS;
    return (asleep / CAT_SNORE_MS) % 2 ? CAT_SLEEP_1 : CAT_SLEEP_0;
}
