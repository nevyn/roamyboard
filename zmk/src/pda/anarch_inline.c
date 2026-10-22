/*
 * The runner of anarch.h for the host: pda_anarch_take_frame() runs the game in the caller,
 * for the time that pda_anarch_feed() says has passed.
 *
 * SPDX-License-Identifier: MIT
 */

#include "anarch.h"

static uint32_t fed_ms, run_ms;
static uint16_t fed_keys;
static bool fed, running;

void pda_anarch_feed(uint32_t now_ms, uint16_t keys) {
    if (!fed || now_ms - fed_ms > PDA_ANARCH_LEASE_MS) {
        running = false;
    }
    fed = true;
    fed_ms = now_ms;
    fed_keys = keys;
}

bool pda_anarch_take_frame(uint8_t frame[PDA_ANARCH_BYTES]) {
    if (!fed) {
        return false;
    }
    // Resuming after a pause starts the clock again, as the keyboard's runner does.
    const uint32_t elapsed = running ? fed_ms - run_ms : 0;
    running = true;
    run_ms = fed_ms;
    return pda_anarch_game_run(elapsed, fed_keys, frame);
}

void pda_anarch_load(const uint8_t save[PDA_ANARCH_SAVE_SIZE]) { pda_anarch_game_load(save); }

bool pda_anarch_take_save(uint8_t save[PDA_ANARCH_SAVE_SIZE]) {
    return pda_anarch_game_take_save(save);
}
