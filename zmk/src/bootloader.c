/*
 * Reboots into the Adafruit nRF52 bootloader's UF2 or OTA mode after the status screen shows
 * the bootloader view. docs/firmware.md describes why the OTA magic goes through the
 * zmk,magic-boot-mode retention partition.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <dt-bindings/zmk/reset.h>

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
#include <zephyr/retention/bootmode.h>
#endif
#if DT_HAS_CHOSEN(zmk_magic_boot_mode)
#include <zephyr/retention/retention.h>
#endif

#include <roamyboard/bootloader.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/** Longest wait for the status screen to write the bootloader view before rebooting anyway. */
#define SCREEN_TIMEOUT_MS 1000

/** The Adafruit nRF52 bootloader starts Bluetooth DFU when GPREGRET holds this value. */
#define ADAFRUIT_OTA_MAGIC 0xA8

/** The mode of the last request; read by the reboot that follows. */
static enum roamyboard_bootloader_mode pending_mode;

static void reboot_to_uf2(void) {
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

static void reboot_to_ota(void) {
#if DT_HAS_CHOSEN(zmk_magic_boot_mode)
    // Zephyr boot modes have no OTA mode; write the magic to the partition that ZMK's mapper uses.
    const struct device *magic = DEVICE_DT_GET(DT_CHOSEN(zmk_magic_boot_mode));
    const uint8_t value = ADAFRUIT_OTA_MAGIC;
    const int err = retention_write(magic, 0, &value, sizeof(value));
    if (err < 0) {
        LOG_ERR("Failed to write the OTA magic 0x%02x to %s (%d); not rebooting", value,
                magic->name, err);
        return;
    }
    sys_reboot(SYS_REBOOT_WARM);
#else
    LOG_ERR("No zmk,magic-boot-mode retention partition in the devicetree; cannot enter the "
            "OTA bootloader");
#endif
}

static void reboot_to_pending_mode(void) {
    switch (pending_mode) {
    case ROAMYBOARD_BOOTLOADER_UF2:
        reboot_to_uf2();
        break;
    case ROAMYBOARD_BOOTLOADER_OTA:
        reboot_to_ota();
        break;
    }
}

static void screen_timeout_handler(struct k_work *work) {
    LOG_WRN("The status screen did not show the bootloader view within %d ms; rebooting "
            "without it",
            SCREEN_TIMEOUT_MS);
    reboot_to_pending_mode();
}

static K_WORK_DELAYABLE_DEFINE(screen_timeout, screen_timeout_handler);

void roamyboard_enter_bootloader(enum roamyboard_bootloader_mode mode) {
    pending_mode = mode;
#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN)
    const int err = roamyboard_status_screen_show_bootloader(mode, reboot_to_pending_mode);
    if (err == 0) {
        k_work_schedule(&screen_timeout, K_MSEC(SCREEN_TIMEOUT_MS));
        return;
    }
    LOG_WRN("Cannot show the bootloader view (%d); rebooting without it", err);
#endif
    reboot_to_pending_mode();
}
