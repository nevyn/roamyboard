/*
 * Parameters of &midi_ctl, which changes how the MIDI keys play. docs/midi.md describes them.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** Every &midi_note one octave down, to at most 3 octaves below the keymap's notes. */
#define MIDI_OCT_DN 0
/** Every &midi_note one octave up, to at most 3 octaves above. */
#define MIDI_OCT_UP 1
/** Every &midi_note one semitone down, to at most 11 below. */
#define MIDI_TR_DN 2
/** Every &midi_note one semitone up, to at most 11 above. */
#define MIDI_TR_UP 3
/** The next softer velocity step: ff 127, f 110, mf 90, p 64, pp 40. */
#define MIDI_VEL_DN 4
/** The next louder velocity step. */
#define MIDI_VEL_UP 5
/** &midi_note and &midi_cc on the channel below, down to channel 1. */
#define MIDI_CH_DN 6
/** &midi_note and &midi_cc on the channel above, up to channel 16. */
#define MIDI_CH_UP 7
/** Note off for every held MIDI key, then All Notes Off and sustain off on every channel. */
#define MIDI_PANIC 8
