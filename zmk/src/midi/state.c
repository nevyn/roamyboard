/*
 * SPDX-License-Identifier: MIT
 */

#include "state.h"

#include <string.h>

#include <dt-bindings/roamyboard/midi.h>

#define NONE 0xFF

static const uint8_t velocities[MIDI_VELOCITY_STEPS] = {40, 64, 90, 110, 127};

void midi_state_init(struct midi_state *s) {
    memset(s, 0, sizeof(*s));
    s->velocity_step = 2;
    s->last_note = -1;
    memset(s->held_note, NONE, sizeof(s->held_note));
}

uint8_t midi_velocity(const struct midi_state *s) { return velocities[s->velocity_step]; }

/** Returns whether it played the note. */
static bool note_on(struct midi_state *s, uint32_t position, int note, uint8_t channel,
                    midi_emit_fn emit, void *ctx) {
    if (position >= MIDI_POSITIONS || note < 0 || note > 127 || s->held_note[position] != NONE) {
        return false;
    }
    s->held_note[position] = (uint8_t)note;
    s->held_channel[position] = channel;
    const uint8_t msg[] = {0x90 | channel, (uint8_t)note, midi_velocity(s)};
    emit(msg, sizeof(msg), ctx);
    return true;
}

void midi_press_note(struct midi_state *s, uint32_t position, uint8_t base_note,
                     midi_emit_fn emit, void *ctx) {
    const int note = base_note + 12 * s->octave + s->transpose;
    if (note_on(s, position, note, s->channel, emit, ctx)) {
        s->last_note = (int16_t)note;
    }
}

void midi_press_drum(struct midi_state *s, uint32_t position, uint8_t note, midi_emit_fn emit,
                     void *ctx) {
    note_on(s, position, note, MIDI_DRUM_CHANNEL, emit, ctx);
}

void midi_release(struct midi_state *s, uint32_t position, midi_emit_fn emit, void *ctx) {
    if (position >= MIDI_POSITIONS || s->held_note[position] == NONE) {
        return;
    }
    const uint8_t msg[] = {0x80 | s->held_channel[position], s->held_note[position], 0};
    s->held_note[position] = NONE;
    emit(msg, sizeof(msg), ctx);
}

void midi_held_notes(const struct midi_state *s, uint8_t bits[16]) {
    memset(bits, 0, 16);
    for (uint32_t position = 0; position < MIDI_POSITIONS; position++) {
        const uint8_t note = s->held_note[position];
        if (note != NONE && s->held_channel[position] != MIDI_DRUM_CHANNEL) {
            bits[note / 8] |= 1 << (note % 8);
        }
    }
}

void midi_control_change(const struct midi_state *s, uint8_t cc, uint8_t value,
                         midi_emit_fn emit, void *ctx) {
    const uint8_t msg[] = {0xB0 | s->channel, cc & 0x7F, value & 0x7F};
    emit(msg, sizeof(msg), ctx);
}

static int8_t step(int8_t value, int delta, int max) {
    const int next = value + delta;
    return (int8_t)(next < -max ? -max : next > max ? max : next);
}

bool midi_apply_control(struct midi_state *s, uint32_t control, midi_emit_fn emit, void *ctx) {
    switch (control) {
    case MIDI_OCT_DN:
    case MIDI_OCT_UP:
        s->octave = step(s->octave, control == MIDI_OCT_UP ? 1 : -1, MIDI_OCTAVE_MAX);
        return true;
    case MIDI_TR_DN:
    case MIDI_TR_UP:
        s->transpose = step(s->transpose, control == MIDI_TR_UP ? 1 : -1, MIDI_TRANSPOSE_MAX);
        return true;
    case MIDI_VEL_DN:
        if (s->velocity_step > 0) {
            s->velocity_step--;
        }
        return true;
    case MIDI_VEL_UP:
        if (s->velocity_step < MIDI_VELOCITY_STEPS - 1) {
            s->velocity_step++;
        }
        return true;
    case MIDI_CH_DN:
        if (s->channel > 0) {
            s->channel--;
        }
        return true;
    case MIDI_CH_UP:
        if (s->channel < 15) {
            s->channel++;
        }
        return true;
    case MIDI_PANIC:
        for (uint32_t position = 0; position < MIDI_POSITIONS; position++) {
            midi_release(s, position, emit, ctx);
        }
        for (uint8_t channel = 0; channel < 16; channel++) {
            const uint8_t all_notes_off[] = {0xB0 | channel, 123, 0};
            const uint8_t sustain_off[] = {0xB0 | channel, 64, 0};
            emit(all_notes_off, sizeof(all_notes_off), ctx);
            emit(sustain_off, sizeof(sustain_off), ctx);
        }
        return true;
    default:
        return false;
    }
}
