/*
 * SPDX-License-Identifier: MIT
 */

#include "note.h"

#include <string.h>

#include "keys.h"

void pda_note_init(struct pda_note *note) { memset(note, 0, sizeof(*note)); }

void pda_note_clear(struct pda_note *note) {
    note->len = 0;
    note->text[0] = '\0';
}

void pda_note_set_text(struct pda_note *note, const char *text, size_t len) {
    if (len > PDA_NOTE_MAX) {
        len = PDA_NOTE_MAX;
        // Back off to the start of the sequence that the cut lands in.
        while (len > 0 && ((uint8_t)text[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(note->text, text, len);
    note->text[len] = '\0';
    note->len = len;
}

void pda_note_reset_keys(struct pda_note *note) {
    note->held_mods = 0;
    note->dead = PDA_DEAD_NONE;
}

static enum pda_note_action append(struct pda_note *note, uint32_t cp) {
    char buf[2];
    size_t n;
    if (cp < 0x80) {
        buf[0] = (char)cp;
        n = 1;
    } else {
        buf[0] = (char)(0xC0 | cp >> 6);
        buf[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    }
    if (note->len + n > PDA_NOTE_MAX) {
        return PDA_NOTE_NONE;
    }
    memcpy(note->text + note->len, buf, n);
    note->len += n;
    note->text[note->len] = '\0';
    return PDA_NOTE_CHANGED;
}

static enum pda_note_action delete_last(struct pda_note *note) {
    if (note->len == 0) {
        return PDA_NOTE_NONE;
    }
    do {
        note->len--;
    } while (note->len > 0 && ((uint8_t)note->text[note->len] & 0xC0) == 0x80);
    note->text[note->len] = '\0';
    return PDA_NOTE_CHANGED;
}

/** The letter with the dead key's mark, or c itself when the mark does not combine with it. */
static uint32_t compose(enum pda_dead_key dead, char c) {
    switch (dead) {
    case PDA_DEAD_RING:
        return c == 'a' ? 0xE5 : c == 'A' ? 0xC5 : (uint32_t)c;
    case PDA_DEAD_UMLAUT:
        switch (c) {
        case 'a':
            return 0xE4;
        case 'A':
            return 0xC4;
        case 'o':
            return 0xF6;
        case 'O':
            return 0xD6;
        }
        return (uint32_t)c;
    case PDA_DEAD_NONE:
        break;
    }
    return (uint32_t)c;
}

enum pda_note_action pda_note_key(struct pda_note *note, uint8_t usage, uint8_t implicit_mods,
                                  bool pressed) {
    if (usage >= PDA_USAGE_LEFT_CONTROL && usage <= PDA_USAGE_RIGHT_GUI) {
        const uint8_t bit = (uint8_t)(1u << (usage - PDA_USAGE_LEFT_CONTROL));
        note->held_mods = pressed ? (note->held_mods | bit) : (note->held_mods & ~bit);
        return PDA_NOTE_NONE;
    }
    if (!pressed) {
        return PDA_NOTE_NONE;
    }

    const uint8_t mods = note->held_mods | implicit_mods;
    const bool enter = usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER;
    const enum pda_dead_key dead = note->dead;
    note->dead = PDA_DEAD_NONE;

    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_NOTE_LEAVE;
    }
    if (mods & PDA_MODS_GUI) {
        if (enter) {
            return PDA_NOTE_TYPE_OUT;
        }
        if (usage == PDA_USAGE_BACKSPACE && note->len > 0) {
            pda_note_clear(note);
            return PDA_NOTE_CHANGED;
        }
        return PDA_NOTE_NONE;
    }
    if (mods & PDA_MODS_CTRL) {
        return PDA_NOTE_NONE;
    }
    if (mods & PDA_MODS_ALT) {
        note->dead = usage == PDA_USAGE_K   ? PDA_DEAD_RING
                     : usage == PDA_USAGE_U ? PDA_DEAD_UMLAUT
                                            : PDA_DEAD_NONE;
        return PDA_NOTE_NONE;
    }
    if (usage == PDA_USAGE_BACKSPACE) {
        return delete_last(note);
    }
    if (enter) {
        return append(note, '\n');
    }
    if (usage == PDA_USAGE_TAB) {
        return append(note, '\t');
    }
    const char c = pda_keys_char(usage, mods & PDA_MODS_SHIFT);
    return c ? append(note, compose(dead, c)) : PDA_NOTE_NONE;
}
