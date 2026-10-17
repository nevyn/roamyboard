/*
 * What the MIDI keys play: octave, transpose, velocity, channel, and the note that each held
 * key sent. Pure C, so zmk/tests drives it on the host. docs/midi.md describes the keys.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Key positions that can hold a note: 5 rows of 24 keymap columns. */
#define MIDI_POSITIONS 120
#define MIDI_OCTAVE_MAX 3
#define MIDI_TRANSPOSE_MAX 11
#define MIDI_VELOCITY_STEPS 5
/** Channel 10, as a 0-based channel number: General MIDI's drums. */
#define MIDI_DRUM_CHANNEL 9

/** Receives each MIDI message that a call produces, status byte first. */
typedef void (*midi_emit_fn)(const uint8_t *msg, size_t len, void *ctx);

struct midi_state {
    int8_t octave;
    int8_t transpose;
    /** Index into the velocity steps, softest first. */
    uint8_t velocity_step;
    /** 0-based: 0 is channel 1. */
    uint8_t channel;
    /** The MIDI note number of the latest note on, or -1 before any. */
    int16_t last_note;
    /** Per key position: the note that it holds, or 0xFF. */
    uint8_t held_note[MIDI_POSITIONS];
    /** Per key position: the channel of that note. */
    uint8_t held_channel[MIDI_POSITIONS];
};

/** Octave 0, transpose 0, velocity mf, channel 1, nothing held. */
void midi_state_init(struct midi_state *s);

/** The velocity of the current step, 40 to 127. */
uint8_t midi_velocity(const struct midi_state *s);

/**
 * Note on for base_note shifted by the octave and transpose, on the current channel, held by
 * position until midi_release(). Emits nothing when the shifted note falls outside 0 to 127,
 * when position is out of range, or when position already holds a note.
 */
void midi_press_note(struct midi_state *s, uint32_t position, uint8_t base_note,
                     midi_emit_fn emit, void *ctx);

/** Note on for note as it is, on channel 10, held by position until midi_release(). */
void midi_press_drum(struct midi_state *s, uint32_t position, uint8_t note, midi_emit_fn emit,
                     void *ctx);

/** Note off for the note that position holds, on the channel that it was played on, if any. */
void midi_release(struct midi_state *s, uint32_t position, midi_emit_fn emit, void *ctx);

/** Control change cc (0 to 127) to value (0 to 127) on the current channel. */
void midi_control_change(const struct midi_state *s, uint8_t cc, uint8_t value,
                         midi_emit_fn emit, void *ctx);

/**
 * Applies one of the MIDI_* controls of dt-bindings/roamyboard/midi.h. Changes clamp at their
 * limits. MIDI_PANIC emits note off for every held key and releases them, then All Notes Off
 * (CC 123) and sustain off (CC 64) on all 16 channels.
 * @return false for an unknown control.
 */
bool midi_apply_control(struct midi_state *s, uint32_t control, midi_emit_fn emit, void *ctx);
