/*
 * The MIDI keys: &midi_note, &midi_drum, &midi_ctl (roamyboard,behavior-midi-key) and
 * &midi_cc (roamyboard,behavior-midi-cc). docs/midi.md describes them.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <dt-bindings/roamyboard/midi.h>
#include <roamyboard/midi.h>

#include "../midi/state.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/** Keymap behaviors all run on the same thread, so the state needs no lock. */
static struct midi_state state;
static bool state_ready;

static struct midi_state *midi(void) {
    if (!state_ready) {
        midi_state_init(&state);
        state_ready = true;
    }
    return &state;
}

static void emit(const uint8_t *msg, size_t len, void *ctx) {
    const int64_t *timestamp = ctx;
    const int err = roamyboard_midi_send(msg, len, *timestamp);
    if (err) {
        LOG_WRN("MIDI %02x %02x not sent: %d", msg[0], len > 1 ? msg[1] : 0, err);
    }
}

/** The binding's kind enum lists "note", "drum" then "ctl". */
enum kind { KIND_NOTE, KIND_DRUM, KIND_CTL };

struct behavior_midi_key_config {
    enum kind kind;
};

static int midi_key_pressed(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_midi_key_config *config = dev->config;
    const uint32_t p = binding->param1;

    if (config->kind != KIND_CTL && p > 127) {
        LOG_WRN("%s %u at position %u is not a MIDI note", binding->behavior_dev, p,
                event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }
    switch (config->kind) {
    case KIND_NOTE:
        midi_press_note(midi(), event.position, (uint8_t)p, emit, &event.timestamp);
        break;
    case KIND_DRUM:
        midi_press_drum(midi(), event.position, (uint8_t)p, emit, &event.timestamp);
        break;
    case KIND_CTL:
        if (!midi_apply_control(midi(), p, emit, &event.timestamp)) {
            LOG_WRN("&midi_ctl %u at position %u is not a MIDI control", p, event.position);
        }
        break;
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static int midi_key_released(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event) {
    midi_release(midi(), event.position, emit, &event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int midi_cc_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    midi_control_change(midi(), binding->param1, binding->param2, emit, &event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int midi_cc_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    midi_control_change(midi(), binding->param1, 0, emit, &event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

#define METADATA(name, ...)                                                                        \
    static const struct behavior_parameter_value_metadata name##_values[] = {__VA_ARGS__};         \
    static const struct behavior_parameter_metadata_set name##_set[] = {{                          \
        .param1_values = name##_values,                                                            \
        .param1_values_len = ARRAY_SIZE(name##_values),                                            \
    }};                                                                                            \
    static const struct behavior_parameter_metadata name##_metadata = {                            \
        .sets_len = 1,                                                                             \
        .sets = name##_set,                                                                        \
    };

METADATA(note, {.display_name = "Note",
                .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
                .range = {.min = 0, .max = 127}})

#define CTL(control, name)                                                                        \
    {.display_name = name, .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = control}
METADATA(ctl, CTL(MIDI_OCT_DN, "Octave down"), CTL(MIDI_OCT_UP, "Octave up"),
         CTL(MIDI_TR_DN, "Transpose down"), CTL(MIDI_TR_UP, "Transpose up"),
         CTL(MIDI_VEL_DN, "Softer"), CTL(MIDI_VEL_UP, "Louder"),
         CTL(MIDI_CH_DN, "Channel down"), CTL(MIDI_CH_UP, "Channel up"),
         CTL(MIDI_PANIC, "Panic"))

static const struct behavior_parameter_value_metadata cc_values[] = {
    {.display_name = "Controller", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
     .range = {.min = 0, .max = 127}},
};
static const struct behavior_parameter_value_metadata cc_value_values[] = {
    {.display_name = "Value", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
     .range = {.min = 0, .max = 127}},
};
static const struct behavior_parameter_metadata_set cc_set[] = {{
    .param1_values = cc_values,
    .param1_values_len = ARRAY_SIZE(cc_values),
    .param2_values = cc_value_values,
    .param2_values_len = ARRAY_SIZE(cc_value_values),
}};
static const struct behavior_parameter_metadata cc_metadata = {
    .sets_len = 1,
    .sets = cc_set,
};

#define API(name, pressed, released)                                                               \
    static const struct behavior_driver_api name##_api = {                                         \
        .binding_pressed = pressed,                                                                \
        .binding_released = released,                                                              \
        .locality = BEHAVIOR_LOCALITY_CENTRAL,                                                     \
        .parameter_metadata = &name##_metadata,                                                    \
    };

#else

#define API(name, pressed, released)                                                               \
    static const struct behavior_driver_api name##_api = {                                         \
        .binding_pressed = pressed,                                                                \
        .binding_released = released,                                                              \
        .locality = BEHAVIOR_LOCALITY_CENTRAL,                                                     \
    };

#endif

API(note, midi_key_pressed, midi_key_released)
API(ctl, midi_key_pressed, midi_key_released)
API(cc, midi_cc_pressed, midi_cc_released)

#define DT_DRV_COMPAT roamyboard_behavior_midi_key

#define MIDI_KEY_INST(n)                                                                           \
    static const struct behavior_midi_key_config midi_key_config_##n = {                           \
        .kind = (enum kind)DT_INST_ENUM_IDX(n, kind),                                              \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &midi_key_config_##n, POST_KERNEL,                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            DT_INST_ENUM_IDX(n, kind) == KIND_CTL ? &ctl_api : &note_api);

DT_INST_FOREACH_STATUS_OKAY(MIDI_KEY_INST)

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT roamyboard_behavior_midi_cc

#define MIDI_CC_INST(n)                                                                            \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &cc_api);

DT_INST_FOREACH_STATUS_OKAY(MIDI_CC_INST)
