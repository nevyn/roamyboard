/*
 * The PDA's mapping between HID keyboard usages and text, for the layout that the keymaps
 * assume on the host: macOS ABC - Extended, which is US ASCII plus Option dead keys. Pure C,
 * so that zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** HID keyboard page usage IDs that the PDA treats specially. */
enum {
    PDA_USAGE_A = 0x04,
    PDA_USAGE_K = 0x0E,
    PDA_USAGE_O = 0x12,
    PDA_USAGE_U = 0x18,
    PDA_USAGE_RETURN = 0x28,
    PDA_USAGE_ESCAPE = 0x29,
    PDA_USAGE_BACKSPACE = 0x2A,
    PDA_USAGE_TAB = 0x2B,
    PDA_USAGE_KP_ENTER = 0x58,
    PDA_USAGE_LEFT_CONTROL = 0xE0,
    PDA_USAGE_RIGHT_GUI = 0xE7,
};

/** Modifier bits, as in a HID boot keyboard report: bit n is usage 0xE0 + n. */
enum {
    PDA_MOD_LCTRL = 0x01,
    PDA_MOD_LSHIFT = 0x02,
    PDA_MOD_LALT = 0x04,
    PDA_MOD_LGUI = 0x08,
    PDA_MOD_RCTRL = 0x10,
    PDA_MOD_RSHIFT = 0x20,
    PDA_MOD_RALT = 0x40,
    PDA_MOD_RGUI = 0x80,
    PDA_MODS_CTRL = PDA_MOD_LCTRL | PDA_MOD_RCTRL,
    PDA_MODS_SHIFT = PDA_MOD_LSHIFT | PDA_MOD_RSHIFT,
    PDA_MODS_ALT = PDA_MOD_LALT | PDA_MOD_RALT,
    PDA_MODS_GUI = PDA_MOD_LGUI | PDA_MOD_RGUI,
};

/** One key press and release, with the modifiers held around it. */
struct pda_tap {
    uint8_t usage;
    /** PDA_MOD_* bits. */
    uint8_t mods;
};

/**
 * Returns the ASCII character that a key types on the host, or 0 for a key that types none
 * (Return, Backspace, modifiers, F-keys). Keypad keys type their digit or operator whatever
 * the shift state, since the keyboard does not track Num Lock.
 */
char pda_keys_char(uint8_t usage, bool shift);

/**
 * Fills taps with the key presses that type a Unicode code point on the host: one tap for
 * ASCII, Tab and newline (Return), and two for å, ä, ö, Å, Ä and Ö (an Option dead key, then
 * the letter).
 *
 * @return The number of taps, 0 if the code point cannot be typed.
 */
size_t pda_keys_taps(uint32_t codepoint, struct pda_tap taps[2]);

/**
 * Decodes the UTF-8 sequence at text[*pos], advances *pos past it and returns its code
 * point. A malformed or cut-short sequence yields 0xFFFD and advances by one byte.
 */
uint32_t pda_utf8_next(const char *text, size_t len, size_t *pos);
