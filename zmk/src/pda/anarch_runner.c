/*
 * The runner of anarch.h on the keyboard: a thread at the lowest priority runs the game while
 * the app feeds it, so that a game frame never holds up the work queues.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>

#include "anarch.h"

#define FRAME_MS 40
/** Longest game time that one frame catches up on, after a stall. */
#define MAX_STEP_MS 200
#define STACK_SIZE 4096

static K_MUTEX_DEFINE(game_lock);
static K_SEM_DEFINE(wake, 0, 1);
static K_MUTEX_DEFINE(io_lock);

/* Guarded by io_lock, a mutex: a spinlock would mask the radio's interrupts during the copy. */
static uint32_t fed_ms;
static uint16_t fed_keys;
static bool running;
static bool fresh;
static uint8_t front[PDA_ANARCH_BYTES];

void pda_anarch_feed(uint32_t now_ms, uint16_t keys) {
    k_mutex_lock(&io_lock, K_FOREVER);
    fed_ms = now_ms;
    fed_keys = keys;
    const bool start = !running;
    running = true;
    k_mutex_unlock(&io_lock);
    if (start) {
        k_sem_give(&wake);
    }
}

bool pda_anarch_take_frame(uint8_t frame[PDA_ANARCH_BYTES]) {
    k_mutex_lock(&io_lock, K_FOREVER);
    const bool had = fresh;
    if (had) {
        memcpy(frame, front, PDA_ANARCH_BYTES);
        fresh = false;
    }
    k_mutex_unlock(&io_lock);
    return had;
}

void pda_anarch_load(const uint8_t save[PDA_ANARCH_SAVE_SIZE]) {
    k_mutex_lock(&game_lock, K_FOREVER);
    pda_anarch_game_load(save);
    k_mutex_unlock(&game_lock);
}

bool pda_anarch_take_save(uint8_t save[PDA_ANARCH_SAVE_SIZE]) {
    k_mutex_lock(&game_lock, K_FOREVER);
    const bool saved = pda_anarch_game_take_save(save);
    k_mutex_unlock(&game_lock);
    return saved;
}

/** Whether the lease still holds; ends the run if not. The keys held come out in keys. */
static bool still_fed(uint16_t *keys) {
    k_mutex_lock(&io_lock, K_FOREVER);
    if (k_uptime_get_32() - fed_ms > PDA_ANARCH_LEASE_MS) {
        running = false;
    }
    const bool fed = running;
    *keys = fed_keys;
    k_mutex_unlock(&io_lock);
    return fed;
}

static void run(void *a, void *b, void *c) {
    ARG_UNUSED(a), ARG_UNUSED(b), ARG_UNUSED(c);
    static uint8_t back[PDA_ANARCH_BYTES];
    for (;;) {
        k_sem_take(&wake, K_FOREVER);
        // A resumed game starts from where it paused, without catching up on the pause.
        uint32_t last = k_uptime_get_32();
        uint16_t keys;
        while (still_fed(&keys)) {
            const uint32_t start = k_uptime_get_32();
            const uint32_t elapsed = MIN(start - last, MAX_STEP_MS);
            last = start;
            k_mutex_lock(&game_lock, K_FOREVER);
            const bool drew = pda_anarch_game_run(elapsed, keys, back);
            k_mutex_unlock(&game_lock);
            if (drew) {
                k_mutex_lock(&io_lock, K_FOREVER);
                memcpy(front, back, PDA_ANARCH_BYTES);
                fresh = true;
                k_mutex_unlock(&io_lock);
            }
            const uint32_t spent = k_uptime_get_32() - start;
            k_msleep(spent < FRAME_MS ? FRAME_MS - spent : 1);
        }
    }
}

K_THREAD_DEFINE(pda_anarch_thread, STACK_SIZE, run, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
