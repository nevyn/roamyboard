/*
 * What the keymap does at a key position now. Central and unibody only: the split peripheral
 * has no keymap.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/behavior.h>

/**
 * The binding that a press at position triggers with the layers active now: the binding of the
 * highest active layer that is not &trans there, or the default layer's. NULL for a position
 * outside the keymap.
 */
const struct zmk_behavior_binding *roamyboard_keymap_binding_at(uint32_t position);

/**
 * Whether binding runs the behavior whose device name is behavior, as DEVICE_DT_NAME() gives it,
 * for example DEVICE_DT_NAME(DT_NODELABEL(bt)).
 */
bool roamyboard_binding_is(const struct zmk_behavior_binding *binding, const char *behavior);
