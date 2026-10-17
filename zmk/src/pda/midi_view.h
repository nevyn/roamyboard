/*
 * Draws the MIDI view: the latest note, the octave, transpose, velocity and channel, and three
 * octaves of piano keys with the held notes filled. Pure C. docs/midi.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "gfx.h"
#include "shell.h"

/** The geometry of the keys, for the tests: three octaves stacked, the highest on top. */
#define PDA_MIDI_KEYS_Y 92
#define PDA_MIDI_OCTAVE_H 22
#define PDA_MIDI_WHITE_W 9
#define PDA_MIDI_KEYS_X 2

/** The lowest note drawn: C3 moved by the octave. */
int pda_midi_view_lowest(const struct pda_midi_view *view);

void pda_midi_view_draw(const struct pda_midi_view *view, struct pda_fb *fb,
                        struct pda_rect area);
