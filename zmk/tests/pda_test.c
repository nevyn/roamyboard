/*
 * Host tests for zmk/src/pda/keys.c and note.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "keys.h"
#include "note.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define CHECK_STR(a, b)                                                                            \
    do {                                                                                           \
        const char *a_ = (a), *b_ = (b);                                                           \
        if (strcmp(a_, b_) != 0) {                                                                 \
            fprintf(stderr, "%s:%d: CHECK_STR failed in %s: \"%s\", expected \"%s\"\n", __FILE__,  \
                    __LINE__, __func__, a_, b_);                                                   \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

enum {
    LSHIFT = 0xE1,
    LGUI = 0xE3,
    SPACE = 0x2C,
    N1 = 0x1E,
    SEMI = 0x33,
    KP_N7 = 0x5F,
};

static enum pda_note_action tap(struct pda_note *note, uint8_t usage, uint8_t implicit_mods) {
    const enum pda_note_action action = pda_note_key(note, usage, implicit_mods, true);
    pda_note_key(note, usage, implicit_mods, false);
    return action;
}

/** Feeds the taps that type text, as the central would forward them, and returns the note. */
static const char *round_trip(struct pda_note *note, const char *text) {
    pda_note_init(note);
    size_t pos = 0;
    const size_t len = strlen(text);
    while (pos < len) {
        const uint32_t cp = pda_utf8_next(text, len, &pos);
        struct pda_tap taps[2];
        const size_t n = pda_keys_taps(cp, taps);
        CHECK(n > 0);
        for (size_t i = 0; i < n; i++) {
            tap(note, taps[i].usage, taps[i].mods);
        }
    }
    return note->text;
}

static void test_letters_and_shift(void) {
    struct pda_note note;
    pda_note_init(&note);
    CHECK(tap(&note, PDA_USAGE_A, 0) == PDA_NOTE_CHANGED);
    pda_note_key(&note, LSHIFT, 0, true);
    tap(&note, PDA_USAGE_A, 0);
    tap(&note, N1, 0);
    pda_note_key(&note, LSHIFT, 0, false);
    tap(&note, SPACE, 0);
    tap(&note, SEMI, 0);
    tap(&note, KP_N7, 0);
    CHECK_STR(note.text, "aA! ;7");
}

static void test_keymap_dead_key_macros(void) {
    // &aring_u: &kp LA(K), then &kp LS(A).
    struct pda_note note;
    pda_note_init(&note);
    tap(&note, PDA_USAGE_K, PDA_MOD_LALT);
    tap(&note, PDA_USAGE_A, PDA_MOD_LSHIFT);
    tap(&note, PDA_USAGE_U, PDA_MOD_LALT);
    tap(&note, PDA_USAGE_O, 0);
    CHECK_STR(note.text, "Åö");
    // A dead key that does not combine leaves the letter alone, and lasts one key only.
    tap(&note, PDA_USAGE_K, PDA_MOD_LALT);
    tap(&note, PDA_USAGE_O, 0);
    tap(&note, PDA_USAGE_A, 0);
    CHECK_STR(note.text, "Åöoa");
}

static void test_backspace_removes_whole_characters(void) {
    struct pda_note note;
    round_trip(&note, "aä");
    CHECK(tap(&note, PDA_USAGE_BACKSPACE, 0) == PDA_NOTE_CHANGED);
    CHECK_STR(note.text, "a");
    tap(&note, PDA_USAGE_BACKSPACE, 0);
    CHECK(tap(&note, PDA_USAGE_BACKSPACE, 0) == PDA_NOTE_NONE);
    CHECK_STR(note.text, "");
}

static void test_commands(void) {
    struct pda_note note;
    round_trip(&note, "hej");
    CHECK(tap(&note, PDA_USAGE_ESCAPE, 0) == PDA_NOTE_LEAVE);
    CHECK(tap(&note, PDA_USAGE_RETURN, PDA_MOD_LGUI) == PDA_NOTE_TYPE_OUT);
    CHECK_STR(note.text, "hej");
    pda_note_key(&note, LGUI, 0, true);
    CHECK(tap(&note, PDA_USAGE_BACKSPACE, 0) == PDA_NOTE_CHANGED);
    pda_note_key(&note, LGUI, 0, false);
    CHECK_STR(note.text, "");
    // Control shortcuts type nothing.
    CHECK(tap(&note, PDA_USAGE_A, PDA_MOD_LCTRL) == PDA_NOTE_NONE);
    CHECK_STR(note.text, "");
}

static void test_reset_forgets_held_modifiers(void) {
    struct pda_note note;
    pda_note_init(&note);
    pda_note_key(&note, LSHIFT, 0, true);
    pda_note_reset_keys(&note);
    tap(&note, PDA_USAGE_A, 0);
    CHECK_STR(note.text, "a");
}

static void test_full_note_takes_no_more(void) {
    struct pda_note note;
    pda_note_init(&note);
    for (int i = 0; i < PDA_NOTE_MAX - 1; i++) {
        tap(&note, PDA_USAGE_A, 0);
    }
    // One byte left: a two-byte ä does not fit, an a does.
    CHECK(tap(&note, PDA_USAGE_U, PDA_MOD_LALT) == PDA_NOTE_NONE);
    CHECK(tap(&note, PDA_USAGE_A, 0) == PDA_NOTE_NONE);
    CHECK(tap(&note, PDA_USAGE_A, 0) == PDA_NOTE_CHANGED);
    CHECK(note.len == PDA_NOTE_MAX);
    CHECK(tap(&note, PDA_USAGE_A, 0) == PDA_NOTE_NONE);
}

static void test_round_trip_every_typable_character(void) {
    char ascii[0x7F - 0x20 + 1];
    for (int c = 0x20; c < 0x7F; c++) {
        ascii[c - 0x20] = (char)c;
    }
    ascii[sizeof(ascii) - 1] = '\0';
    struct pda_note note;
    CHECK_STR(round_trip(&note, ascii), ascii);
    CHECK_STR(round_trip(&note, "Räksmörgås\nÅÄÖ\tåäö"), "Räksmörgås\nÅÄÖ\tåäö");
}

static void test_untypable_characters(void) {
    struct pda_note note;
    struct pda_tap taps[2];
    CHECK(pda_keys_taps(0x20AC, taps) == 0); // €
    CHECK(pda_keys_taps('\r', taps) == 0);
    (void)note;
}

static void test_set_text_cuts_at_whole_sequence(void) {
    char long_text[PDA_NOTE_MAX + 2];
    memset(long_text, 'a', PDA_NOTE_MAX - 1);
    // An ä whose second byte lies past the limit.
    long_text[PDA_NOTE_MAX - 1] = (char)0xC3;
    long_text[PDA_NOTE_MAX] = (char)0xA4;
    long_text[PDA_NOTE_MAX + 1] = '\0';
    struct pda_note note;
    pda_note_init(&note);
    pda_note_set_text(&note, long_text, PDA_NOTE_MAX + 1);
    CHECK(note.len == PDA_NOTE_MAX - 1);
}

static void test_utf8_malformed(void) {
    const char cut[] = {(char)0xC3};
    size_t pos = 0;
    CHECK(pda_utf8_next(cut, 1, &pos) == 0xFFFD);
    CHECK(pos == 1);
    const char bad[] = {(char)0xC3, 'a'};
    pos = 0;
    CHECK(pda_utf8_next(bad, 2, &pos) == 0xFFFD);
    CHECK(pos == 1);
}

int main(void) {
    test_letters_and_shift();
    test_keymap_dead_key_macros();
    test_backspace_removes_whole_characters();
    test_commands();
    test_reset_forgets_held_modifiers();
    test_full_note_takes_no_more();
    test_round_trip_every_typable_character();
    test_untypable_characters();
    test_set_text_cuts_at_whole_sequence();
    test_utf8_malformed();
    if (failures) {
        fprintf(stderr, "pda_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_test: all checks passed\n");
    return 0;
}
