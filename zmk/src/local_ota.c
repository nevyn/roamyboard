/*
 * The split peripheral's local OTA key: while the peripheral has no connection to the central,
 * holding the key at CONFIG_ROAMYBOARD_LOCAL_OTA_POSITION for CONFIG_ROAMYBOARD_LOCAL_OTA_HOLD_MS
 * enters OTA mode, as &ota_boot does. docs/firmware.md, "Updating over Bluetooth".
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>

#include <roamyboard/bootloader.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void hold_expired(struct k_work *work) {
    if (zmk_split_bt_peripheral_is_connected()) {
        return;
    }
    LOG_INF("Key %d held for %d ms without the central; entering OTA mode",
            CONFIG_ROAMYBOARD_LOCAL_OTA_POSITION, CONFIG_ROAMYBOARD_LOCAL_OTA_HOLD_MS);
    roamyboard_enter_bootloader(ROAMYBOARD_BOOTLOADER_OTA);
}

static K_WORK_DELAYABLE_DEFINE(hold_work, hold_expired);

static int local_ota_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *position = as_zmk_position_state_changed(eh);
    if (position != NULL) {
        if (position->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL ||
            position->position != CONFIG_ROAMYBOARD_LOCAL_OTA_POSITION) {
            return ZMK_EV_EVENT_BUBBLE;
        }
        if (position->state && !zmk_split_bt_peripheral_is_connected()) {
            k_work_reschedule(&hold_work, K_MSEC(CONFIG_ROAMYBOARD_LOCAL_OTA_HOLD_MS));
        } else if (!position->state) {
            k_work_cancel_delayable(&hold_work);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_split_peripheral_status_changed *status =
        as_zmk_split_peripheral_status_changed(eh);
    if (status != NULL && status->connected) {
        k_work_cancel_delayable(&hold_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_local_ota, local_ota_listener);
ZMK_SUBSCRIPTION(roamyboard_local_ota, zmk_position_state_changed);
ZMK_SUBSCRIPTION(roamyboard_local_ota, zmk_split_peripheral_status_changed);
