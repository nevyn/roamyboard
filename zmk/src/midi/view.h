/*
 * The MIDI view: the central tells the PDA on the split peripheral what the MIDI keys play,
 * while the highest active layer has MIDI keys. docs/midi.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** The MIDI keys changed what they play or hold. System work queue only. */
void roamyboard_midi_view_update(void);

/** Sends the view again, for a split link that has just come up. Any thread. */
void roamyboard_midi_view_resend(void);
