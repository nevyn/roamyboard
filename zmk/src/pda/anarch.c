/*
 * The game layer of anarch.h: Anarch's frontend, which dithers the game's colours to the
 * PDA's 1-bit frame.
 *
 * SPDX-License-Identifier: MIT
 */

#include "anarch.h"

#include <string.h>

#define SFG_SCREEN_RESOLUTION_X PDA_ANARCH_W
#define SFG_SCREEN_RESOLUTION_Y PDA_ANARCH_H
#define SFG_FPS 25
#define SFG_CAN_EXIT 0
#define SFG_DIMINISH_SPRITES 1
#define SFG_RAYCASTING_MAX_HITS 7

// Anarch stays as upstream wrote it, warnings included.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "anarch/game.h"
#pragma GCC diagnostic pop

_Static_assert(PDA_ANARCH_KEY_COUNT == SFG_KEY_COUNT, "pda_anarch_key follows SFG_KEY_*");
_Static_assert(PDA_ANARCH_KEY_CYCLE_WEAPON == SFG_KEY_CYCLE_WEAPON,
               "pda_anarch_key follows SFG_KEY_*");
_Static_assert(PDA_ANARCH_SAVE_SIZE == SFG_SAVE_SIZE, "the save's size is the game's");
_Static_assert(PDA_ANARCH_W % 8 == 0, "rows are whole bytes");

static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/** Each palette colour's brightness, 0 to 16, which the 4 x 4 Bayer matrix dithers. */
static uint8_t level[256];

static bool started;
static uint32_t game_ms;
static uint16_t held;
static uint8_t *target;
static bool drew;

static uint8_t save_data[SFG_SAVE_SIZE];
static bool save_loaded;
static bool save_changed;

static void init_levels(void) {
    for (int i = 0; i < 256; i++) {
        const uint16_t c = paletteRGB565[i];
        const uint32_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63;
        const uint32_t b = (c & 31) * 255 / 31;
        const uint32_t luma = (299 * r + 587 * g + 114 * b) / 1000;
        level[i] = (uint8_t)((luma * 17) / 256);
    }
}

static inline void SFG_setPixel(uint16_t x, uint16_t y, uint8_t colorIndex) {
    uint8_t *byte = &target[y * PDA_ANARCH_STRIDE + x / 8];
    const uint8_t bit = (uint8_t)(0x80 >> (x % 8));
    *byte = level[colorIndex] > bayer[y & 3][x & 3] ? (*byte | bit) : (*byte & ~bit);
    drew = true;
}

uint32_t SFG_getTimeMs(void) { return game_ms; }

// The runner paces the game; Anarch sleeps only when called before its next step is due.
void SFG_sleepMs(uint16_t timeMs) { (void)timeMs; }

int8_t SFG_keyPressed(uint8_t key) { return key < SFG_KEY_COUNT && (held >> key) & 1; }

void SFG_getMouseOffset(int16_t *x, int16_t *y) { (void)x, (void)y; }

void SFG_playSound(uint8_t soundIndex, uint8_t volume) { (void)soundIndex, (void)volume; }

void SFG_setMusic(uint8_t value) { (void)value; }

void SFG_processEvent(uint8_t event, uint8_t data) { (void)event, (void)data; }

void SFG_save(uint8_t data[SFG_SAVE_SIZE]) {
    memcpy(save_data, data, SFG_SAVE_SIZE);
    save_loaded = true;
    save_changed = true;
}

uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE]) {
    if (save_loaded) {
        memcpy(data, save_data, SFG_SAVE_SIZE);
    }
    return 1;
}

bool pda_anarch_game_run(uint32_t elapsed_ms, uint16_t keys, uint8_t frame[PDA_ANARCH_BYTES]) {
    target = frame;
    if (!started) {
        init_levels();
        SFG_init();
        started = true;
    }
    game_ms += elapsed_ms;
    held = keys;
    drew = false;
    SFG_mainLoopBody();
    return drew;
}

void pda_anarch_game_load(const uint8_t save[PDA_ANARCH_SAVE_SIZE]) {
    memcpy(save_data, save, SFG_SAVE_SIZE);
    save_loaded = true;
}

bool pda_anarch_game_take_save(uint8_t save[PDA_ANARCH_SAVE_SIZE]) {
    if (!save_changed) {
        return false;
    }
    memcpy(save, save_data, SFG_SAVE_SIZE);
    save_changed = false;
    return true;
}
