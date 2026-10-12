/*
 * &pda_sel and &pda_note: enter or leave PDA mode. docs/pda.md describes them.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT roamyboard_behavior_pda

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <roamyboard/pda.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_pda_config {
    enum roamyboard_pda_mode mode;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_pda_config *config = dev->config;

    const int err = roamyboard_pda_select(config->mode);
    if (err) {
        LOG_WRN("%s did not change PDA mode: %d", binding->behavior_dev, err);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_pda_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

// The binding's mode enum lists "home" then "note".
BUILD_ASSERT(ROAMYBOARD_PDA_HOME == 1 && ROAMYBOARD_PDA_NOTE == 2,
             "roamyboard_pda_mode must follow the binding's mode enum, offset by one");

#define BEHAVIOR_PDA_INST(n)                                                                       \
    static const struct behavior_pda_config behavior_pda_config_##n = {                            \
        .mode = (enum roamyboard_pda_mode)(DT_INST_ENUM_IDX(n, mode) + 1),                         \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_pda_config_##n, POST_KERNEL,            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_pda_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BEHAVIOR_PDA_INST)
