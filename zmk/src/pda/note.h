/*
 * The PDA's note editor: turns the key events that the central forwards into the note's
 * UTF-8 text. Pure C, so that zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** The longest note in bytes of UTF-8, without the terminating NUL. */
#define PDA_NOTE_MAX 512

enum pda_dead_key {
    PDA_DEAD_NONE,
    /** Option-K: the next a or A becomes å or Å. */
    PDA_DEAD_RING,
    /** Option-U: the next a, o, A or O gets an umlaut. */
    PDA_DEAD_UMLAUT,
};

struct pda_note {
    /** NUL-terminated UTF-8. */
    char text[PDA_NOTE_MAX + 1];
    size_t len;
    /** PDA_MOD_* bits of the modifier keys held now. */
    uint8_t held_mods;
    enum pda_dead_key dead;
};

/** What the caller must do after a key event. */
enum pda_note_action {
    PDA_NOTE_NONE,
    /** The text changed; redraw it. */
    PDA_NOTE_CHANGED,
    /** Cmd-Return: type the text out to the host, then clear it. */
    PDA_NOTE_TYPE_OUT,
    /** Escape: leave PDA mode. The text stays. */
    PDA_NOTE_LEAVE,
};

/** Empties the note and forgets held keys. */
void pda_note_init(struct pda_note *note);

/** Replaces the text, cut to PDA_NOTE_MAX bytes at a whole UTF-8 sequence. */
void pda_note_set_text(struct pda_note *note, const char *text, size_t len);

/** Empties the text. */
void pda_note_clear(struct pda_note *note);

/**
 * Forgets held modifiers and a pending dead key, for when key events stop arriving: on
 * entering or leaving PDA mode, a held key's release may never come.
 */
void pda_note_reset_keys(struct pda_note *note);

/**
 * Applies one key event of the HID keyboard page.
 *
 * Letters, digits, punctuation, Space and Tab type themselves, with Shift. Return and keypad
 * Enter start a new line. Backspace deletes the last character, Cmd-Backspace the whole
 * text. Option-K and Option-U are the dead keys that the keymaps' å, ä and ö macros send.
 * Other shortcuts with Control, Option or Cmd do nothing. A key that would overflow
 * PDA_NOTE_MAX does nothing.
 *
 * @param usage HID keyboard usage ID; 0xE0 to 0xE7 are the modifiers.
 * @param implicit_mods PDA_MOD_* bits that the binding adds to this key only, as for &kp LA(K).
 * @param pressed True for a press, false for a release.
 */
enum pda_note_action pda_note_key(struct pda_note *note, uint8_t usage, uint8_t implicit_mods,
                                  bool pressed);
