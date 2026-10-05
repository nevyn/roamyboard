/*
 * &boot_screen: shows the bootloader view on the status screen, then reboots into the UF2
 * bootloader like ZMK's &bootloader. docs/firmware.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT roamyboard_behavior_boot_screen

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/reset.h>
#include <zmk/behavior.h>

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
#include <zephyr/retention/bootmode.h>
#endif

#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN)
#include <roamyboard/status_screen.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/** Longest wait for the status screen to write the bootloader view before rebooting anyway. */
#define SCREEN_TIMEOUT_MS 1000

static void reboot_to_bootloader(void) {
#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
    const int err = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
    if (err < 0) {
        LOG_ERR("Failed to set the bootloader boot mode (%d); not rebooting", err);
        return;
    }
    sys_reboot(SYS_REBOOT_WARM);
#else
    // The Adafruit nRF52 bootloader enters UF2 mode on this GPREGRET value.
    sys_reboot(RST_UF2);
#endif
}

static void screen_timeout_handler(struct k_work *work) {
    LOG_WRN("The status screen did not show the bootloader view within %d ms; rebooting "
            "without it",
            SCREEN_TIMEOUT_MS);
    reboot_to_bootloader();
}

static K_WORK_DELAYABLE_DEFINE(screen_timeout, screen_timeout_handler);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN)
    const int err = roamyboard_status_screen_show_bootloader(reboot_to_bootloader);
    if (err == 0) {
        k_work_schedule(&screen_timeout, K_MSEC(SCREEN_TIMEOUT_MS));
        return ZMK_BEHAVIOR_OPAQUE;
    }
    LOG_WRN("Cannot show the bootloader view (%d); rebooting without it", err);
#endif
    reboot_to_bootloader();
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_boot_screen_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
             "one &boot_screen node is enough; bindings share it");

#define BOOT_SCREEN_INST(n)                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_boot_screen_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BOOT_SCREEN_INST)
