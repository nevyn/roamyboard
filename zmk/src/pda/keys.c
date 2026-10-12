/*
 * SPDX-License-Identifier: MIT
 */

#include "keys.h"

/** Usages 0x1E (1) to 0x38 (/): unshifted and shifted characters, 0 where none. */
#define FIRST_SYMBOL_USAGE 0x1E
static const char symbol_plain[] = "1234567890\0\0\0\0 -=[]\\\0;'`,./";
static const char symbol_shifted[] = "!@#$%^&*()\0\0\0\0 _+{}|\0:\"~<>?";
#define SYMBOL_COUNT (sizeof(symbol_plain) - 1)

/** Usages 0x54 (keypad /) to 0x63 (keypad .); 0 for keypad Enter. */
#define FIRST_KEYPAD_USAGE 0x54
static const char keypad[] = "/*-+\0" "1234567890.";
#define KEYPAD_COUNT (sizeof(keypad) - 1)

_Static_assert(sizeof(symbol_plain) == sizeof(symbol_shifted), "one shifted symbol per key");
_Static_assert(FIRST_SYMBOL_USAGE + SYMBOL_COUNT - 1 == 0x38, "symbols end at the / key");
_Static_assert(FIRST_KEYPAD_USAGE + KEYPAD_COUNT - 1 == 0x63, "keypad ends at the . key");

char pda_keys_char(uint8_t usage, bool shift) {
    if (usage >= PDA_USAGE_A && usage < PDA_USAGE_A + 26) {
        return (char)((shift ? 'A' : 'a') + (usage - PDA_USAGE_A));
    }
    if (usage >= FIRST_SYMBOL_USAGE && usage < FIRST_SYMBOL_USAGE + SYMBOL_COUNT) {
        return (shift ? symbol_shifted : symbol_plain)[usage - FIRST_SYMBOL_USAGE];
    }
    if (usage >= FIRST_KEYPAD_USAGE && usage < FIRST_KEYPAD_USAGE + KEYPAD_COUNT) {
        return keypad[usage - FIRST_KEYPAD_USAGE];
    }
    return 0;
}

size_t pda_keys_taps(uint32_t codepoint, struct pda_tap taps[2]) {
    switch (codepoint) {
    case '\n':
        taps[0] = (struct pda_tap){PDA_USAGE_RETURN, 0};
        return 1;
    case '\t':
        taps[0] = (struct pda_tap){PDA_USAGE_TAB, 0};
        return 1;
    case 0xE5: // å
    case 0xC5: // Å
        taps[0] = (struct pda_tap){PDA_USAGE_K, PDA_MOD_LALT};
        taps[1] = (struct pda_tap){PDA_USAGE_A, codepoint == 0xC5 ? PDA_MOD_LSHIFT : 0};
        return 2;
    case 0xE4: // ä
    case 0xC4: // Ä
    case 0xF6: // ö
    case 0xD6: // Ö
        taps[0] = (struct pda_tap){PDA_USAGE_U, PDA_MOD_LALT};
        taps[1] = (struct pda_tap){(codepoint == 0xE4 || codepoint == 0xC4) ? PDA_USAGE_A
                                                                             : PDA_USAGE_O,
                                   (codepoint == 0xC4 || codepoint == 0xD6) ? PDA_MOD_LSHIFT : 0};
        return 2;
    }
    if (codepoint < 0x20 || codepoint > 0x7E) {
        return 0;
    }
    // Keypad keys come last in the search, so that digits and operators use the main keys.
    for (int shift = 0; shift <= 1; shift++) {
        for (uint8_t usage = PDA_USAGE_A; usage < FIRST_SYMBOL_USAGE + SYMBOL_COUNT; usage++) {
            if (pda_keys_char(usage, shift) == (char)codepoint) {
                taps[0] = (struct pda_tap){usage, shift ? PDA_MOD_LSHIFT : 0};
                return 1;
            }
        }
    }
    return 0;
}

uint32_t pda_utf8_next(const char *text, size_t len, size_t *pos) {
    const uint8_t lead = (uint8_t)text[*pos];
    size_t need;
    uint32_t cp;
    if (lead < 0x80) {
        *pos += 1;
        return lead;
    } else if ((lead & 0xE0) == 0xC0) {
        need = 1;
        cp = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 2;
        cp = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        need = 3;
        cp = lead & 0x07;
    } else {
        *pos += 1;
        return 0xFFFD;
    }
    if (*pos + need >= len) {
        *pos += 1;
        return 0xFFFD;
    }
    for (size_t i = 1; i <= need; i++) {
        const uint8_t b = (uint8_t)text[*pos + i];
        if ((b & 0xC0) != 0x80) {
            *pos += 1;
            return 0xFFFD;
        }
        cp = cp << 6 | (b & 0x3F);
    }
    *pos += need + 1;
    return cp;
}
