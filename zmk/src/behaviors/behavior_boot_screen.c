/*
 * &boot_screen and &ota_boot: show a bootloader view on the status screen, then reboot into
 * the Adafruit nRF52 bootloader's UF2 or OTA mode. docs/firmware.md describes them.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT roamyboard_behavior_boot_screen

#include <zephyr/device.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <roamyboard/bootloader.h>

struct boot_screen_config {
    enum roamyboard_bootloader_mode mode;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct boot_screen_config *config = dev->config;

    roamyboard_enter_bootloader(config->mode);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_boot_screen_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BUILD_ASSERT(ROAMYBOARD_BOOTLOADER_UF2 == 0 && ROAMYBOARD_BOOTLOADER_OTA == 1,
             "roamyboard_bootloader_mode must follow the order of the mode enum in the binding");

#define BOOT_SCREEN_INST(n)                                                                        \
    static const struct boot_screen_config boot_screen_config_##n = {                              \
        .mode = (enum roamyboard_bootloader_mode)DT_INST_ENUM_IDX(n, mode),                        \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &boot_screen_config_##n, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_boot_screen_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BOOT_SCREEN_INST)
