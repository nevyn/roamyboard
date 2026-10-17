/*
 * The MIDI keys' state, which behavior_midi.c changes and view.c shows.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#include "state.h"

/** What the MIDI keys play now. System work queue only, as the keymap's behaviors run there. */
const struct midi_state *roamyboard_midi_state(void);

/** Whether behavior_dev, a binding's behavior name, is one of the MIDI keys. */
bool roamyboard_midi_is_key(const char *behavior_dev);
