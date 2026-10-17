/*
 * &midi_note: a MIDI note on channel 1 while the key is held. docs/midi.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT roamyboard_behavior_midi_note

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <roamyboard/midi.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define VELOCITY 100

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {
        .display_name = "Note",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
        .range = {.min = 0, .max = 127},
    },
};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

static int send_note(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event,
                     bool on) {
    if (binding->param1 > 127) {
        LOG_WRN("&midi_note %u at position %u is not a MIDI note", binding->param1,
                event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }
    const uint8_t msg[] = {on ? 0x90 : 0x80, (uint8_t)binding->param1, on ? VELOCITY : 0};
    const int err = roamyboard_midi_send(msg, sizeof(msg), event.timestamp);
    if (err) {
        LOG_WRN("MIDI note %u %s not sent: %d", binding->param1, on ? "on" : "off", err);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return send_note(binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return send_note(binding, event, false);
}

static const struct behavior_driver_api behavior_midi_note_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

#define BEHAVIOR_MIDI_NOTE_INST(n)                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_midi_note_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BEHAVIOR_MIDI_NOTE_INST)
