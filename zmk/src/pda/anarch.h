/*
 * Anarch, the game in zmk/src/pda/anarch/, behind two layers: the game (pda_anarch_game_*),
 * which one thread runs, and the runner, which carries keys in and frames and saves out across
 * threads for the PDA app. docs/pda.md describes the app.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** The game's frame: PDA_ANARCH_H rows top to bottom, PDA_ANARCH_STRIDE bytes per row, MSB
 * leftmost, a set bit light. The game is landscape; the app shows it sideways. */
#define PDA_ANARCH_W 160
#define PDA_ANARCH_H 68
#define PDA_ANARCH_STRIDE (PDA_ANARCH_W / 8)
#define PDA_ANARCH_BYTES (PDA_ANARCH_STRIDE * PDA_ANARCH_H)

/** The game's save: unlocked levels, settings and totals, as the game encodes them. */
#define PDA_ANARCH_SAVE_SIZE 12

/** The game's keys, as bit numbers of the key mask: bit n held means key n is down. */
enum pda_anarch_key {
    PDA_ANARCH_KEY_UP,
    PDA_ANARCH_KEY_RIGHT,
    PDA_ANARCH_KEY_DOWN,
    PDA_ANARCH_KEY_LEFT,
    /** Fire; confirm in menus. */
    PDA_ANARCH_KEY_A,
    /** Cancel in menus; held, Left and Right strafe, and Up and Down look if free look is on. */
    PDA_ANARCH_KEY_B,
    /** Held with a direction: menu, jump, weapon switching. */
    PDA_ANARCH_KEY_C,
    PDA_ANARCH_KEY_JUMP,
    PDA_ANARCH_KEY_STRAFE_LEFT,
    PDA_ANARCH_KEY_STRAFE_RIGHT,
    PDA_ANARCH_KEY_MAP,
    PDA_ANARCH_KEY_TOGGLE_FREELOOK,
    PDA_ANARCH_KEY_NEXT_WEAPON,
    PDA_ANARCH_KEY_PREVIOUS_WEAPON,
    PDA_ANARCH_KEY_MENU,
    PDA_ANARCH_KEY_CYCLE_WEAPON,
    PDA_ANARCH_KEY_COUNT,
};

/* The game. Not thread-safe: the runner calls these, from one thread at a time. */

/**
 * Advances the game by elapsed_ms of game time with the keys in the mask held, then draws a
 * frame into frame if at least one game step passed. The game clock only moves in these
 * calls, so the game waits between them; a long elapsed_ms runs every step it covers. The
 * first call starts the game with the save from pda_anarch_game_load(), if any.
 * @return Whether it drew a frame; the first calls, before the menu shows, draw none.
 */
bool pda_anarch_game_run(uint32_t elapsed_ms, uint16_t keys, uint8_t frame[PDA_ANARCH_BYTES]);

/** Gives the game the save that settings keep; takes effect when the game starts. */
void pda_anarch_game_load(const uint8_t save[PDA_ANARCH_SAVE_SIZE]);

/**
 * Copies the game's latest save into save.
 * @return Whether the game saved since the last call; save is unchanged if not.
 */
bool pda_anarch_game_take_save(uint8_t save[PDA_ANARCH_SAVE_SIZE]);

/* The runner, which is thread-safe. On the keyboard, a thread of its own runs the game
 * (anarch_runner.c); on the host, pda_anarch_take_frame() runs it in the caller
 * (anarch_inline.c). */

/**
 * Keeps the game running with the keys in the mask held. The game runs only while this keeps
 * coming: it pauses PDA_ANARCH_LEASE_MS after the last call, and resumes without catching up.
 */
void pda_anarch_feed(uint32_t now_ms, uint16_t keys);

/** How long one pda_anarch_feed() keeps the game running. */
#define PDA_ANARCH_LEASE_MS 100

/**
 * Copies the newest frame into frame.
 * @return Whether there was a frame newer than the last one taken; frame is unchanged if not.
 */
bool pda_anarch_take_frame(uint8_t frame[PDA_ANARCH_BYTES]);

/** pda_anarch_game_load(), safe to call while the game runs. */
void pda_anarch_load(const uint8_t save[PDA_ANARCH_SAVE_SIZE]);

/** pda_anarch_game_take_save(), safe to call while the game runs. */
bool pda_anarch_take_save(uint8_t save[PDA_ANARCH_SAVE_SIZE]);
