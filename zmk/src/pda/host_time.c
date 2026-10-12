/*
 * The PDA's clock on the central: reads the Current Time characteristic (0x2A2B) from the
 * connected hosts, one profile after another until one answers, whenever a host connection
 * is encrypted and every hour. docs/pda.md, Clock.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/ble.h>

#include "central.h"
#include "clock.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define REFRESH K_HOURS(1)

/** Guards the clock below, which pda_host_time_get() reads from any thread. */
static K_MUTEX_DEFINE(lock);
static enum pda_clock_status status = PDA_CLOCK_UNKNOWN;
static uint32_t base_seconds;
static int64_t base_ms;
static uint8_t source_profile;

/* A round tries each connected host in profile order. System work queue and the read
 * callback only; busy is set while a read is in flight. */
static struct bt_gatt_read_params params;
static atomic_t busy;
/** A host connected: the next round starts from the first profile. */
static atomic_t restart;
static int next_profile;
static bool round_asked;
static bool round_failed;

enum pda_clock_status pda_host_time_get(uint32_t *local_seconds, uint8_t *profile) {
    k_mutex_lock(&lock, K_FOREVER);
    const enum pda_clock_status now_status = status;
    *local_seconds = base_seconds + (uint32_t)((k_uptime_get() - base_ms) / 1000);
    *profile = source_profile;
    k_mutex_unlock(&lock);
    return now_status;
}

static void set_status(enum pda_clock_status new_status) {
    k_mutex_lock(&lock, K_FOREVER);
    status = new_status;
    k_mutex_unlock(&lock);
    pda_central_send_state();
}

static void round_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(round_work, round_handler);

/** Any thread; a read in flight finishes first, and its callback resumes the round. */
static void start_round(void) {
    atomic_set(&restart, true);
    k_work_reschedule(&round_work, K_NO_WAIT);
}

static uint8_t read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *read,
                       const void *data, uint16_t length) {
    const int profile = next_profile;
    uint32_t seconds;
    if (err) {
        if (err == BT_ATT_ERR_ATTRIBUTE_NOT_FOUND) {
            LOG_INF("Profile %d's host has no Current Time Service", profile);
        } else {
            LOG_WRN("Reading profile %d's current time failed: ATT error 0x%02x", profile, err);
            round_failed = true;
        }
    } else if (data != NULL && pda_clock_from_cts(data, length, &seconds)) {
        k_mutex_lock(&lock, K_FOREVER);
        base_seconds = seconds;
        base_ms = k_uptime_get();
        source_profile = (uint8_t)profile;
        k_mutex_unlock(&lock);
        const struct pda_date_time t = pda_clock_split(seconds);
        LOG_INF("Profile %d's host time: %04d-%02d-%02d %02d:%02d:%02d", profile, t.year,
                t.month, t.day, t.hour, t.minute, t.second);
        next_profile = ZMK_BLE_PROFILE_COUNT;
        atomic_clear(&busy);
        set_status(PDA_CLOCK_SET);
        k_work_reschedule(&round_work, REFRESH);
        return BT_GATT_ITER_STOP;
    } else {
        LOG_WRN("Profile %d's host sent a malformed current time of %u bytes", profile, length);
        round_failed = true;
    }
    next_profile++;
    atomic_clear(&busy);
    k_work_reschedule(&round_work, K_NO_WAIT);
    return BT_GATT_ITER_STOP;
}

/** The connection to profile's host, if it is up and encrypted; the caller unrefs it. */
static struct bt_conn *host_conn(int profile) {
    if (zmk_ble_profile_is_open(profile)) {
        return NULL;
    }
    struct bt_conn *conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, zmk_ble_profile_address(profile));
    if (conn == NULL) {
        return NULL;
    }
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) == 0 && info.state == BT_CONN_STATE_CONNECTED &&
        info.role == BT_CONN_ROLE_PERIPHERAL && bt_conn_get_security(conn) >= BT_SECURITY_L2) {
        return conn;
    }
    bt_conn_unref(conn);
    return NULL;
}

static void round_handler(struct k_work *work) {
    if (atomic_get(&busy)) {
        return;
    }
    if (atomic_clear(&restart) || next_profile >= ZMK_BLE_PROFILE_COUNT) {
        // A host connected, or the hourly refresh: begin a new round.
        next_profile = 0;
        round_asked = false;
        round_failed = false;
    }
    for (; next_profile < ZMK_BLE_PROFILE_COUNT; next_profile++) {
        struct bt_conn *conn = host_conn(next_profile);
        if (conn == NULL) {
            continue;
        }
        params = (struct bt_gatt_read_params){
            .func = read_cb,
            .handle_count = 0,
            .by_uuid =
                {
                    .uuid = BT_UUID_CTS_CURRENT_TIME,
                    .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
                    .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
                },
        };
        atomic_set(&busy, true);
        round_asked = true;
        const int err = bt_gatt_read(conn, &params);
        bt_conn_unref(conn);
        if (err == 0) {
            return;
        }
        atomic_clear(&busy);
        LOG_WRN("Reading profile %d's current time could not start: %d", next_profile, err);
        round_failed = true;
    }

    // Every connected host was asked, and none had the time.
    k_mutex_lock(&lock, K_FOREVER);
    const bool was_set = status == PDA_CLOCK_SET;
    k_mutex_unlock(&lock);
    if (round_asked && !was_set) {
        set_status(round_failed ? PDA_CLOCK_ERROR : PDA_CLOCK_NO_CTS);
    }
    k_work_reschedule(&round_work, REFRESH);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    struct bt_conn_info info;
    if (err || level < BT_SECURITY_L2 || bt_conn_get_info(conn, &info) != 0 ||
        info.role != BT_CONN_ROLE_PERIPHERAL) {
        return;
    }
    start_round();
}

BT_CONN_CB_DEFINE(roamyboard_pda_host_time) = {
    .security_changed = security_changed,
};
