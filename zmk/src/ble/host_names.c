/*
 * Reads each Bluetooth host's GAP Device Name once per encrypted connection, and keeps the
 * latest name of every BLE profile in settings. roamyboard/host_name.h has the contract;
 * docs/firmware.md describes when the reads happen.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

#include <roamyboard/events/host_name_changed.h>
#include <roamyboard/host_name.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(roamyboard_host_name_changed);

BUILD_ASSERT(ZMK_BLE_PROFILE_COUNT <= ATOMIC_BITS, "one bit per profile in an atomic_t");

#define SETTINGS_PREFIX "roamyboard/host"

struct host {
    /** Guarded by lock. */
    struct roamyboard_host_name name;
    /** The read in flight; owned by the Bluetooth stack while the profile's busy bit is set. */
    struct bt_gatt_read_params params;
    char rx[ROAMYBOARD_HOST_NAME_MAX];
    size_t rx_len;
    /** More bytes arrived than rx holds. */
    bool rx_truncated;
    // The rest is touched on the system work queue only.
    bool seen_paired;
    bool read_sent;
    /** The connection that the latest read went to: its bt_conn_index() and connection count. */
    uint8_t conn_index;
    atomic_val_t conn_count;
};

static struct host hosts[ZMK_BLE_PROFILE_COUNT];
static K_MUTEX_DEFINE(lock);

/** One bit per profile. */
static atomic_t busy;
static atomic_t changed;
static atomic_t unsaved;

/** Connections made so far per bt_conn_index(), to tell a reconnection from the same one. */
static atomic_t conn_counts[CONFIG_BT_MAX_CONN];

static void reconcile_handler(struct k_work *work);
static K_WORK_DEFINE(reconcile_work, reconcile_handler);
static void publish_handler(struct k_work *work);
static K_WORK_DEFINE(publish_work, publish_handler);

static void set_name(uint8_t profile, enum roamyboard_host_name_state state, int error,
                     const char *name, size_t len) {
    struct roamyboard_host_name *host = &hosts[profile].name;

    k_mutex_lock(&lock, K_FOREVER);
    host->state = state;
    host->error = error;
    memcpy(host->name, name, len);
    host->name[len] = '\0';
    k_mutex_unlock(&lock);

    atomic_set_bit(&changed, profile);
    k_work_submit(&publish_work);
}

int roamyboard_host_name_get(uint8_t profile, struct roamyboard_host_name *out) {
    if (profile >= ZMK_BLE_PROFILE_COUNT) {
        *out = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_NONE};
        return -EINVAL;
    }
    k_mutex_lock(&lock, K_FOREVER);
    *out = hosts[profile].name;
    k_mutex_unlock(&lock);
    return 0;
}

/** Returns the length of buf[0..len) without a UTF-8 sequence that len cuts short. */
static size_t utf8_whole_len(const char *buf, size_t len) {
    size_t start = len;
    while (start > 0 && ((uint8_t)buf[start - 1] & 0xC0) == 0x80) {
        start--;
    }
    if (start == 0) {
        return len;
    }
    const uint8_t lead = (uint8_t)buf[start - 1];
    const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return len - (start - 1) >= need ? len : start - 1;
}

static void finish_read(uint8_t profile) {
    struct host *host = &hosts[profile];

    size_t len = host->rx_truncated ? utf8_whole_len(host->rx, host->rx_len) : host->rx_len;
    while (len > 0 && (host->rx[len - 1] == '\0' || isspace((unsigned char)host->rx[len - 1]))) {
        len--;
    }
    LOG_INF("Profile %u host name: \"%.*s\"%s", profile, (int)len, host->rx,
            host->rx_truncated ? " (cut)" : "");

    atomic_clear_bit(&busy, profile);
    set_name(profile, ROAMYBOARD_HOST_NAME_KNOWN, 0, host->rx, len);
    if (len > 0) {
        atomic_set_bit(&unsaved, profile);
    }
    k_work_submit(&reconcile_work);
}

static void fail_read(uint8_t profile, enum roamyboard_host_name_state state, int error) {
    LOG_WRN("Reading profile %u's host name failed: %s %d", profile,
            state == ROAMYBOARD_HOST_NAME_ATT_ERROR ? "ATT error" : "errno", error);
    atomic_clear_bit(&busy, profile);
    set_name(profile, state, error, "", 0);
    k_work_submit(&reconcile_work);
}

static uint8_t read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
                       const void *data, uint16_t length) {
    struct host *host = CONTAINER_OF(params, struct host, params);
    const uint8_t profile = host - hosts;

    if (err) {
        fail_read(profile, ROAMYBOARD_HOST_NAME_ATT_ERROR, err);
        return BT_GATT_ITER_STOP;
    }
    if (data == NULL) {
        finish_read(profile);
        return BT_GATT_ITER_STOP;
    }

    const size_t room = sizeof(host->rx) - host->rx_len;
    memcpy(host->rx + host->rx_len, data, MIN(length, room));
    host->rx_len += MIN(length, room);
    host->rx_truncated = host->rx_truncated || length > room;

    if (params->handle_count == 0) {
        // Read by UUID returns at most ATT_MTU - 4 bytes; a full response may be cut short,
        // so read the value again by its handle, which continues as a long read.
        if (length >= bt_gatt_get_mtu(conn) - 4 && !host->rx_truncated) {
            const uint16_t handle = params->by_uuid.start_handle;
            host->rx_len = 0;
            params->handle_count = 1;
            params->single.handle = handle;
            params->single.offset = 0;
            const int read_err = bt_gatt_read(conn, params);
            if (read_err) {
                fail_read(profile, ROAMYBOARD_HOST_NAME_READ_ERROR, read_err);
            }
            return BT_GATT_ITER_STOP;
        }
        finish_read(profile);
        return BT_GATT_ITER_STOP;
    }

    if (host->rx_truncated) {
        finish_read(profile);
        return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void start_read(uint8_t profile, struct bt_conn *conn) {
    struct host *host = &hosts[profile];

    host->rx_len = 0;
    host->rx_truncated = false;
    host->params = (struct bt_gatt_read_params){
        .func = read_cb,
        .handle_count = 0,
        .by_uuid =
            {
                .uuid = BT_UUID_GAP_DEVICE_NAME,
                .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
                .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
            },
    };

    atomic_set_bit(&busy, profile);
    set_name(profile, ROAMYBOARD_HOST_NAME_PENDING, 0, "", 0);
    const int err = bt_gatt_read(conn, &host->params);
    if (err) {
        fail_read(profile, ROAMYBOARD_HOST_NAME_READ_ERROR, err);
    }
}

/** Starts a read for every profile whose host is connected and encrypted and not yet read. */
static void reconcile_handler(struct k_work *work) {
    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        struct host *host = &hosts[i];

        if (zmk_ble_profile_is_open(i)) {
            if (host->seen_paired) {
                host->seen_paired = false;
                host->read_sent = false;
                set_name(i, ROAMYBOARD_HOST_NAME_NONE, 0, "", 0);
                atomic_set_bit(&unsaved, i);
            }
            continue;
        }
        host->seen_paired = true;
        if (atomic_test_bit(&busy, i)) {
            continue;
        }

        struct bt_conn *conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, zmk_ble_profile_address(i));
        if (conn == NULL) {
            continue;
        }
        struct bt_conn_info info;
        if (bt_conn_get_info(conn, &info) == 0 && info.state == BT_CONN_STATE_CONNECTED &&
            info.role == BT_CONN_ROLE_PERIPHERAL && bt_conn_get_security(conn) >= BT_SECURITY_L2) {
            const uint8_t index = bt_conn_index(conn);
            const atomic_val_t count = atomic_get(&conn_counts[index]);
            if (!host->read_sent || host->conn_index != index || host->conn_count != count) {
                host->read_sent = true;
                host->conn_index = index;
                host->conn_count = count;
                start_read(i, conn);
            }
        }
        bt_conn_unref(conn);
    }
}

/** Raises the change events and writes changed names to settings. */
static void publish_handler(struct k_work *work) {
    const atomic_val_t changed_bits = atomic_clear(&changed);
    const atomic_val_t unsaved_bits = atomic_clear(&unsaved);

    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        if (unsaved_bits & BIT(i)) {
            char key[sizeof(SETTINGS_PREFIX "/255")];
            snprintf(key, sizeof(key), SETTINGS_PREFIX "/%u", i);
            struct roamyboard_host_name name;
            roamyboard_host_name_get(i, &name);
            const size_t len = strlen(name.name);
            const int err = len ? settings_save_one(key, name.name, len) : settings_delete(key);
            if (err) {
                LOG_ERR("Failed to %s setting %s: %d", len ? "save" : "delete", key, err);
            }
        }
        if (changed_bits & BIT(i)) {
            raise_roamyboard_host_name_changed((struct roamyboard_host_name_changed){.profile = i});
        }
    }
}

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    char *end;
    const unsigned long profile = strtoul(name, &end, 10);
    if (end == name || *end != '\0' || profile >= ZMK_BLE_PROFILE_COUNT || len == 0 ||
        len > ROAMYBOARD_HOST_NAME_MAX) {
        LOG_WRN("Ignoring setting " SETTINGS_PREFIX "/%s of %u bytes: expected a profile index "
                "below %d and 1 to %d bytes",
                name, (unsigned int)len, ZMK_BLE_PROFILE_COUNT, ROAMYBOARD_HOST_NAME_MAX);
        return 0;
    }

    char buf[ROAMYBOARD_HOST_NAME_MAX];
    const ssize_t got = read_cb(cb_arg, buf, len);
    if (got < 0) {
        LOG_ERR("Failed to read setting " SETTINGS_PREFIX "/%s: %d", name, (int)got);
        return (int)got;
    }
    set_name((uint8_t)profile, ROAMYBOARD_HOST_NAME_KNOWN, 0, buf, (size_t)got);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(roamyboard_host, SETTINGS_PREFIX, NULL, settings_set, NULL, NULL);

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err) {
        atomic_inc(&conn_counts[bt_conn_index(conn)]);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    if (!err) {
        k_work_submit(&reconcile_work);
    }
}

BT_CONN_CB_DEFINE(roamyboard_host_names) = {
    .connected = connected,
    .security_changed = security_changed,
};

// A new pairing gets its profile address after the link is encrypted; this catches it.
static int profile_changed_listener(const zmk_event_t *eh) {
    k_work_submit(&reconcile_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_host_names, profile_changed_listener);
ZMK_SUBSCRIPTION(roamyboard_host_names, zmk_ble_active_profile_changed);
