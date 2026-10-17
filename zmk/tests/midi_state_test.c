/*
 * Host tests for zmk/src/midi/state.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "state.h"

#include <stdio.h>
#include <string.h>

#include <dt-bindings/roamyboard/midi.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/** The messages that one call emitted, each padded to 3 bytes. */
struct sent {
    int count;
    uint8_t msgs[64][3];
};

static void collect(const uint8_t *msg, size_t len, void *ctx) {
    struct sent *sent = ctx;
    if (sent->count < 64) {
        memset(sent->msgs[sent->count], 0, 3);
        memcpy(sent->msgs[sent->count], msg, len);
    }
    sent->count++;
}

static bool is(const struct sent *sent, int i, uint8_t a, uint8_t b, uint8_t c) {
    return i < sent->count && sent->msgs[i][0] == a && sent->msgs[i][1] == b &&
           sent->msgs[i][2] == c;
}

static void test_note_on_and_off(void) {
    struct midi_state s;
    midi_state_init(&s);
    struct sent sent = {0};
    midi_press_note(&s, 7, 60, collect, &sent);
    midi_release(&s, 7, collect, &sent);
    CHECK(sent.count == 2);
    CHECK(is(&sent, 0, 0x90, 60, 90));
    CHECK(is(&sent, 1, 0x80, 60, 0));
    CHECK(s.last_note == 60);
}

static void test_release_matches_press_after_octave_change(void) {
    struct midi_state s;
    midi_state_init(&s);
    struct sent sent = {0};
    midi_press_note(&s, 3, 60, collect, &sent);
    midi_apply_control(&s, MIDI_OCT_UP, collect, &sent);
    midi_apply_control(&s, MIDI_CH_UP, collect, &sent);
    midi_release(&s, 3, collect, &sent);
    midi_press_note(&s, 3, 60, collect, &sent);
    CHECK(sent.count == 3);
    CHECK(is(&sent, 1, 0x80, 60, 0));
    CHECK(is(&sent, 2, 0x91, 72, 90));
}

static void test_shift_clamps_and_out_of_range_plays_nothing(void) {
    struct midi_state s;
    midi_state_init(&s);
    for (int i = 0; i < 10; i++) {
        midi_apply_control(&s, MIDI_OCT_UP, collect, NULL);
        midi_apply_control(&s, MIDI_TR_UP, collect, NULL);
    }
    CHECK(s.octave == MIDI_OCTAVE_MAX);
    CHECK(s.transpose == 10);
    for (int i = 0; i < 30; i++) {
        midi_apply_control(&s, MIDI_TR_DN, collect, NULL);
    }
    CHECK(s.transpose == -MIDI_TRANSPOSE_MAX);

    midi_state_init(&s);
    for (int i = 0; i < 3; i++) {
        midi_apply_control(&s, MIDI_OCT_UP, collect, NULL);
    }
    struct sent sent = {0};
    midi_press_note(&s, 0, 120, collect, &sent);
    midi_release(&s, 0, collect, &sent);
    CHECK(sent.count == 0);
}

static void test_drums_ignore_shift_and_channel(void) {
    struct midi_state s;
    midi_state_init(&s);
    midi_apply_control(&s, MIDI_OCT_DN, collect, NULL);
    midi_apply_control(&s, MIDI_TR_UP, collect, NULL);
    midi_apply_control(&s, MIDI_CH_UP, collect, NULL);
    struct sent sent = {0};
    midi_press_drum(&s, 100, 36, collect, &sent);
    midi_release(&s, 100, collect, &sent);
    CHECK(is(&sent, 0, 0x99, 36, 90));
    CHECK(is(&sent, 1, 0x89, 36, 0));
}

static void test_velocity_and_channel_limits(void) {
    struct midi_state s;
    midi_state_init(&s);
    for (int i = 0; i < 9; i++) {
        midi_apply_control(&s, MIDI_VEL_UP, collect, NULL);
        midi_apply_control(&s, MIDI_CH_UP, collect, NULL);
    }
    CHECK(midi_velocity(&s) == 127);
    CHECK(s.channel == 9);
    for (int i = 0; i < 20; i++) {
        midi_apply_control(&s, MIDI_VEL_DN, collect, NULL);
        midi_apply_control(&s, MIDI_CH_DN, collect, NULL);
    }
    CHECK(midi_velocity(&s) == 40);
    CHECK(s.channel == 0);
    for (int i = 0; i < 20; i++) {
        midi_apply_control(&s, MIDI_CH_UP, collect, NULL);
    }
    CHECK(s.channel == 15);
    CHECK(!midi_apply_control(&s, 99, collect, NULL));
}

static void test_held_position_does_not_retrigger(void) {
    struct midi_state s;
    midi_state_init(&s);
    struct sent sent = {0};
    midi_press_note(&s, 5, 60, collect, &sent);
    midi_press_note(&s, 5, 62, collect, &sent);
    midi_press_note(&s, MIDI_POSITIONS, 62, collect, &sent);
    CHECK(sent.count == 1);
}

static void test_control_change_on_current_channel(void) {
    struct midi_state s;
    midi_state_init(&s);
    midi_apply_control(&s, MIDI_CH_UP, collect, NULL);
    struct sent sent = {0};
    midi_control_change(&s, 64, 127, collect, &sent);
    CHECK(is(&sent, 0, 0xB1, 64, 127));
}

static void test_panic_releases_everything(void) {
    struct midi_state s;
    midi_state_init(&s);
    struct sent sent = {0};
    midi_press_note(&s, 1, 60, collect, &sent);
    midi_press_drum(&s, 2, 38, collect, &sent);
    sent.count = 0;
    midi_apply_control(&s, MIDI_PANIC, collect, &sent);
    CHECK(sent.count == 2 + 32);
    CHECK(is(&sent, 0, 0x80, 60, 0));
    CHECK(is(&sent, 1, 0x89, 38, 0));
    CHECK(is(&sent, 2, 0xB0, 123, 0));
    CHECK(is(&sent, 3, 0xB0, 64, 0));
    CHECK(is(&sent, 33, 0xBF, 64, 0));
    sent.count = 0;
    midi_release(&s, 1, collect, &sent);
    CHECK(sent.count == 0);
}

int main(void) {
    test_note_on_and_off();
    test_release_matches_press_after_octave_change();
    test_shift_clamps_and_out_of_range_plays_nothing();
    test_drums_ignore_shift_and_channel();
    test_velocity_and_channel_limits();
    test_held_position_does_not_retrigger();
    test_control_change_on_current_channel();
    test_panic_releases_everything();
    if (failures) {
        fprintf(stderr, "midi_state_test: %d failures\n", failures);
        return 1;
    }
    printf("midi_state_test: all checks passed\n");
    return 0;
}
