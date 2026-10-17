/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

#include "../pda/central.h"
#include "../pda/link.h"
#include "keys.h"
#include "view.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/** Whether the peripheral shows the view. System work queue only. */
static bool shown;

static void send_view(void) {
    const struct midi_state *s = roamyboard_midi_state();
    uint8_t msg[PDA_LINK_MIDI_LEN] = {
        PDA_LINK_MIDI,       shown,      (uint8_t)s->octave,
        (uint8_t)s->transpose, s->velocity_step, s->channel,
        s->last_note < 0 ? 0xFF : (uint8_t)s->last_note,
    };
    uint8_t held[16];
    midi_held_notes(s, held);
    for (int i = 0; i < (PDA_LINK_MIDI_LEN - 7) * 8; i++) {
        const int note = PDA_LINK_MIDI_FIRST_NOTE + i;
        if (held[note / 8] & (1 << (note % 8))) {
            msg[7 + i / 8] |= 1 << (i % 8);
        }
    }
    pda_central_send(msg, sizeof(msg));
}

static void view_handler(struct k_work *work) {
    if (shown) {
        send_view();
    }
}

// Submitting work that is already queued does nothing, so a burst of keys sends one view.
static K_WORK_DEFINE(view_work, view_handler);

void roamyboard_midi_view_update(void) { k_work_submit(&view_work); }

static bool layer_has_midi_keys(zmk_keymap_layer_id_t layer) {
    for (int i = 0; i < ZMK_KEYMAP_LEN; i++) {
        const struct zmk_behavior_binding *binding = zmk_keymap_get_layer_binding_at_idx(layer, i);
        if (binding && binding->behavior_dev && roamyboard_midi_is_key(binding->behavior_dev)) {
            return true;
        }
    }
    return false;
}

static void layer_handler(struct k_work *work) {
    const bool midi = layer_has_midi_keys(
        zmk_keymap_layer_index_to_id(zmk_keymap_highest_layer_active()));
    if (midi == shown) {
        return;
    }
    shown = midi;
    LOG_INF("MIDI view %s", shown ? "on" : "off");
    pda_central_hold_fast(shown);
    send_view();
}

static K_WORK_DEFINE(layer_work, layer_handler);

static void resend_handler(struct k_work *work) {
    if (shown) {
        pda_central_hold_fast(true);
        send_view();
    }
}

static K_WORK_DEFINE(resend_work, resend_handler);

void roamyboard_midi_view_resend(void) { k_work_submit(&resend_work); }

static int layer_listener(const zmk_event_t *eh) {
    k_work_submit(&layer_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_midi_view, layer_listener);
ZMK_SUBSCRIPTION(roamyboard_midi_view, zmk_layer_state_changed);
