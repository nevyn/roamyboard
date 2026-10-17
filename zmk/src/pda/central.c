/*
 * The central's side of the micro PDA: PDA mode, which captures keyboard keys before ZMK's HID
 * listener and forwards them to the peripheral; the GATT client for the split data channel;
 * and typing a note out to the host. docs/pda.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <dt-bindings/zmk/bt.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/battery.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>

#include <roamyboard/build_id.h>
#include <roamyboard/keymap_binding.h>
#include <roamyboard/chain_state.h>
#include <roamyboard/events/chain_state_changed.h>
#include <roamyboard/events/pda_changed.h>
#include <roamyboard/pda.h>

#include "central.h"
#include "keys.h"
#include "link.h"
#include "note.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static atomic_t mode = ATOMIC_INIT(ROAMYBOARD_PDA_OFF);

enum roamyboard_pda_mode roamyboard_pda_mode(void) {
    return (enum roamyboard_pda_mode)atomic_get(&mode);
}

/* Link state. The GATT callbacks run on the Bluetooth receive thread; the rest on the system
 * work queue. lock guards link_conn and to_peripheral_handle. */
static K_MUTEX_DEFINE(lock);
static struct bt_conn *link_conn;
static uint16_t to_peripheral_handle;
static atomic_t link_ready;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_discover_params ccc_discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static uint16_t service_end_handle;

/* Messages to the peripheral, sent in order from the system work queue. */
struct outgoing {
    uint8_t len;
    uint8_t data[PDA_LINK_WRITE_MAX];
};
K_MSGQ_DEFINE(outgoing_msgq, sizeof(struct outgoing), 32, 1);

static void send_handler(struct k_work *work) {
    struct outgoing msg;
    while (k_msgq_get(&outgoing_msgq, &msg, K_NO_WAIT) == 0) {
        k_mutex_lock(&lock, K_FOREVER);
        struct bt_conn *conn = link_conn ? bt_conn_ref(link_conn) : NULL;
        const uint16_t handle = to_peripheral_handle;
        k_mutex_unlock(&lock);
        if (conn == NULL || handle == 0) {
            if (conn) {
                bt_conn_unref(conn);
            }
            LOG_WRN("PDA link down; dropped message 0x%02x", msg.data[0]);
            continue;
        }
        const int err = bt_gatt_write_without_response(conn, handle, msg.data, msg.len, false);
        bt_conn_unref(conn);
        if (err) {
            LOG_ERR("PDA link write of message 0x%02x failed: %d", msg.data[0], err);
        }
    }
}

static K_WORK_DEFINE(send_work, send_handler);

static void send(const uint8_t *data, uint8_t len) {
    struct outgoing msg = {.len = len};
    memcpy(msg.data, data, len);
    if (k_msgq_put(&outgoing_msgq, &msg, K_NO_WAIT) != 0) {
        LOG_WRN("PDA link queue full; dropped message 0x%02x", data[0]);
        return;
    }
    k_work_submit(&send_work);
}

/* Typing out. text_rx collects PDA_LINK_TEXT on the receive thread under lock; the typer
 * copies it into typed when PDA_LINK_TYPE_OUT arrives and runs on the system work queue. */
static char text_rx[PDA_NOTE_MAX];
static size_t text_rx_len;
static char typed[PDA_NOTE_MAX];
static size_t typed_len;
static size_t typed_pos;
static struct pda_tap taps[2];
static size_t tap_count;
static size_t tap_index;
static bool tap_down;

static void typer_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(typer_work, typer_handler);

static void typer_handler(struct k_work *work) {
    while (tap_index >= tap_count) {
        if (typed_pos >= typed_len) {
            return;
        }
        const uint32_t cp = pda_utf8_next(typed, typed_len, &typed_pos);
        tap_count = pda_keys_taps(cp, taps);
        tap_index = 0;
        if (tap_count == 0) {
            LOG_WRN("PDA type-out skips U+%04X, which has no key", cp);
        }
    }

    const struct pda_tap *tap = &taps[tap_index];
    // ZMK's modifier bits are the HID report's, the same as PDA_MOD_*.
    const uint32_t encoded = APPLY_MODS(tap->mods, tap->usage);
    tap_down = !tap_down;
    raise_zmk_keycode_state_changed_from_encoded(encoded, tap_down, k_uptime_get());
    if (!tap_down) {
        tap_index++;
    }
    k_work_schedule(&typer_work, K_MSEC(CONFIG_ROAMYBOARD_PDA_TYPE_OUT_STEP_MS));
}

static void typer_cancel(void) {
    k_work_cancel_delayable(&typer_work);
    typed_len = typed_pos = 0;
    tap_count = tap_index = 0;
    tap_down = false;
}

/*
 * Split connection latency. ZMK lets the peripheral skip up to
 * CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY connection events, so a message from the central waits up
 * to that many intervals; while the PDA has focus, the peripheral listens at every event.
 */
static void latency_handler(struct k_work *work) {
    // The latency that the link has, as far as this half asked for it; a new link starts slow.
    static bool fast;
    const bool focused = roamyboard_pda_mode() != ROAMYBOARD_PDA_OFF;
    if (focused == fast) {
        return;
    }
    k_mutex_lock(&lock, K_FOREVER);
    struct bt_conn *conn = link_conn ? bt_conn_ref(link_conn) : NULL;
    k_mutex_unlock(&lock);
    if (conn == NULL) {
        fast = false;
        return;
    }
    const int err = bt_conn_le_param_update(
        conn, BT_LE_CONN_PARAM(CONFIG_ZMK_SPLIT_BLE_PREF_INT, CONFIG_ZMK_SPLIT_BLE_PREF_INT,
                               focused ? 0 : CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY,
                               CONFIG_ZMK_SPLIT_BLE_PREF_TIMEOUT));
    bt_conn_unref(conn);
    if (err == -EBUSY || err == -EAGAIN || err == -ENOMEM) {
        LOG_DBG("Split connection latency update busy (%d); retrying", err);
        k_work_reschedule(k_work_delayable_from_work(work), K_SECONDS(1));
        return;
    }
    if (err && err != -EALREADY) {
        LOG_WRN("Split connection latency update for PDA focus %d failed: %d", focused, err);
        return;
    }
    fast = focused;
}

static K_WORK_DELAYABLE_DEFINE(latency_work, latency_handler);

/* Mode. Changed on the system work queue only. */

/** The output and profile that the user had chosen when PDA mode began. */
static struct zmk_endpoint_instance chosen_at_entry;

static void set_mode(enum roamyboard_pda_mode new_mode) {
    if (atomic_set(&mode, new_mode) == new_mode) {
        return;
    }
    LOG_INF("PDA mode %d", new_mode);
    send((const uint8_t[]){PDA_LINK_MODE, new_mode}, 2);
    k_work_reschedule(&latency_work, K_NO_WAIT);
    raise_roamyboard_pda_changed((struct roamyboard_pda_changed){});
}

// hid_listener.c defines it with ZMK_SUBSCRIPTION(hid_listener, zmk_keycode_state_changed).
extern const struct zmk_event_subscription zmk_event_sub_hid_listenerzmk_keycode_state_changed;
static int capture_listener(const zmk_event_t *eh);
ZMK_LISTENER(roamyboard_pda, capture_listener);
ZMK_SUBSCRIPTION(roamyboard_pda, zmk_keycode_state_changed);

/** ZMK calls listeners in link order; ours must come before the HID listener's. */
static bool capture_comes_first(void) {
    return &zmk_event_sub_roamyboard_pdazmk_keycode_state_changed <
           &zmk_event_sub_hid_listenerzmk_keycode_state_changed;
}

int roamyboard_pda_select(enum roamyboard_pda_mode new_mode) {
    if (new_mode != ROAMYBOARD_PDA_HOME && new_mode != ROAMYBOARD_PDA_NOTE) {
        return -EINVAL;
    }
    if (roamyboard_pda_mode() == new_mode) {
        set_mode(ROAMYBOARD_PDA_OFF);
        return 0;
    }
    if (!capture_comes_first()) {
        LOG_ERR("PDA mode is off: the PDA's keycode listener links after hid_listener, so "
                "keys would still reach the host");
        return -ENOTSUP;
    }
    if (!atomic_get(&link_ready)) {
        LOG_WRN("PDA mode %d not entered: the peripheral's PDA service is not subscribed",
                new_mode);
        return -ENOTCONN;
    }
    typer_cancel();
    zmk_endpoint_clear_reports();
    chosen_at_entry = zmk_endpoint_get_preferred();
    set_mode(new_mode);
    return 0;
}

static int capture_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    // Consumer keys, such as volume, still reach the host.
    if (ev == NULL || roamyboard_pda_mode() == ROAMYBOARD_PDA_OFF ||
        ev->usage_page != HID_USAGE_KEY || ev->keycode > UINT8_MAX) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    send((const uint8_t[]){PDA_LINK_KEY, ev->state, ev->implicit_modifiers, (uint8_t)ev->keycode},
         PDA_LINK_KEY_LEN);
    return ZMK_EV_EVENT_HANDLED;
}

static atomic_t pressed_position;

/**
 * A key that chooses where the keys go, &bt's profile selection or &out, leaves PDA mode, even
 * for the profile or output already chosen, which raises no ZMK event.
 */
static void output_key_handler(struct k_work *work) {
    if (roamyboard_pda_mode() == ROAMYBOARD_PDA_OFF) {
        return;
    }
    const struct zmk_behavior_binding *binding =
        roamyboard_keymap_binding_at((uint32_t)atomic_get(&pressed_position));
    if (binding == NULL) {
        return;
    }
    const bool profile =
        roamyboard_binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(bt))) &&
        (binding->param1 == BT_SEL_CMD || binding->param1 == BT_NXT_CMD ||
         binding->param1 == BT_PRV_CMD);
    const bool output = roamyboard_binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(out)));
    if (profile || output) {
        set_mode(ROAMYBOARD_PDA_OFF);
    }
}

static K_WORK_DEFINE(output_key_work, output_key_handler);

/** Key positions, for apps such as Keys; the keymap still gets them, to leave PDA mode. */
static int position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || roamyboard_pda_mode() == ROAMYBOARD_PDA_OFF) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->position <= UINT8_MAX) {
        send((const uint8_t[]){PDA_LINK_POSITION, ev->state, (uint8_t)ev->position},
             PDA_LINK_POSITION_LEN);
    }
    if (ev->state) {
        // Hold-tap decides a layer key only after this listener, so look the binding up later.
        atomic_set(&pressed_position, (atomic_val_t)ev->position);
        k_work_submit(&output_key_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_pda_position, position_listener);
ZMK_SUBSCRIPTION(roamyboard_pda_position, zmk_position_state_changed);

/** Sends this half's key module count, battery and build ID, and the clock. */
static void state_handler(struct k_work *work) {
    if (!atomic_get(&link_ready)) {
        return;
    }
    uint8_t msg[PDA_LINK_WRITE_MAX] = {PDA_LINK_INFO,
                                       (uint8_t)(int8_t)roamyboard_chain_key_module_count(),
                                       zmk_battery_state_of_charge()};
    const char *id = roamyboard_build_id();
    const size_t id_len = MIN(strlen(id), sizeof(msg) - 3);
    memcpy(msg + 3, id, id_len);
    send(msg, (uint8_t)(3 + id_len));

    uint32_t seconds;
    uint8_t profile;
    const enum pda_clock_status clock = pda_host_time_get(&seconds, &profile);
    send((const uint8_t[]){PDA_LINK_TIME, clock, profile, seconds & 0xFF, seconds >> 8 & 0xFF,
                           seconds >> 16 & 0xFF, seconds >> 24},
         PDA_LINK_TIME_LEN);
}

static K_WORK_DEFINE(state_work, state_handler);

void pda_central_send_state(void) { k_work_submit(&state_work); }

static int info_listener(const zmk_event_t *eh) {
    pda_central_send_state();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_pda_info, info_listener);
ZMK_SUBSCRIPTION(roamyboard_pda_info, roamyboard_chain_state_changed);
ZMK_SUBSCRIPTION(roamyboard_pda_info, zmk_battery_state_changed);

/**
 * Choosing another output or profile leaves PDA mode, as choosing another device would. ZMK
 * raises the same events when the host connects or disconnects, which must not.
 */
static int output_listener(const zmk_event_t *eh) {
    if (roamyboard_pda_mode() != ROAMYBOARD_PDA_OFF &&
        !zmk_endpoint_instance_eq(zmk_endpoint_get_preferred(), chosen_at_entry)) {
        set_mode(ROAMYBOARD_PDA_OFF);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_pda_output, output_listener);
ZMK_SUBSCRIPTION(roamyboard_pda_output, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(roamyboard_pda_output, zmk_ble_active_profile_changed);

static void leave_handler(struct k_work *work) { set_mode(ROAMYBOARD_PDA_OFF); }
static K_WORK_DEFINE(leave_work, leave_handler);

static void type_out_handler(struct k_work *work) {
    set_mode(ROAMYBOARD_PDA_OFF);
    typer_cancel();
    k_mutex_lock(&lock, K_FOREVER);
    memcpy(typed, text_rx, text_rx_len);
    typed_len = text_rx_len;
    text_rx_len = 0;
    k_mutex_unlock(&lock);
    LOG_INF("PDA types out %u bytes", (unsigned int)typed_len);
    k_work_schedule(&typer_work, K_NO_WAIT);
}

static K_WORK_DEFINE(type_out_work, type_out_handler);

/* GATT client, on the Bluetooth receive thread. */

static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length) {
    if (data == NULL) {
        LOG_INF("PDA link unsubscribed");
        atomic_clear(&link_ready);
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }
    const uint8_t *bytes = data;
    if (length == 0) {
        return BT_GATT_ITER_CONTINUE;
    }
    switch (bytes[0]) {
    case PDA_LINK_TEXT:
        k_mutex_lock(&lock, K_FOREVER);
        if (text_rx_len + length - 1 <= sizeof(text_rx)) {
            memcpy(text_rx + text_rx_len, bytes + 1, length - 1);
            text_rx_len += length - 1;
        } else {
            LOG_ERR("PDA type-out text over %u bytes; dropped %u bytes",
                    (unsigned int)sizeof(text_rx), length - 1);
        }
        k_mutex_unlock(&lock);
        break;
    case PDA_LINK_TYPE_OUT:
        k_work_submit(&type_out_work);
        break;
    case PDA_LINK_LEAVE:
        k_work_submit(&leave_work);
        break;
    default:
        LOG_WRN("PDA link: unknown message 0x%02x of %u bytes", bytes[0], length);
        break;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void subscribed(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params) {
    if (err) {
        LOG_ERR("PDA link subscribe failed: ATT error 0x%02x", err);
        return;
    }
    LOG_INF("PDA link ready");
    atomic_set(&link_ready, true);
    pda_central_send_state();
}

static uint8_t chrc_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        k_mutex_lock(&lock, K_FOREVER);
        const bool found = to_peripheral_handle != 0 && subscribe_params.value_handle != 0;
        k_mutex_unlock(&lock);
        if (!found) {
            LOG_ERR("PDA service lacks a characteristic: to-peripheral %u, to-central %u",
                    to_peripheral_handle, subscribe_params.value_handle);
            return BT_GATT_ITER_STOP;
        }
        subscribe_params.notify = notify_func;
        subscribe_params.subscribe = subscribed;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = 0;
        subscribe_params.disc_params = &ccc_discover_params;
        subscribe_params.end_handle = service_end_handle;
        atomic_set_bit(subscribe_params.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
        const int err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            LOG_ERR("PDA link subscribe failed: %d", err);
        }
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (bt_uuid_cmp(chrc->uuid, BT_UUID_DECLARE_128(PDA_LINK_TO_PERIPHERAL_UUID)) == 0) {
        k_mutex_lock(&lock, K_FOREVER);
        to_peripheral_handle = chrc->value_handle;
        k_mutex_unlock(&lock);
    } else if (bt_uuid_cmp(chrc->uuid, BT_UUID_DECLARE_128(PDA_LINK_TO_CENTRAL_UUID)) == 0) {
        subscribe_params.value_handle = chrc->value_handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t service_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        LOG_WRN("The split peripheral has no PDA service; is its firmware older?");
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_service_val *service = attr->user_data;
    service_end_handle = service->end_handle;
    discover_params.uuid = NULL;
    discover_params.func = chrc_discovered;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = service->end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    const int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_ERR("PDA characteristic discovery failed to start: %d", err);
    }
    return BT_GATT_ITER_STOP;
}

static bool is_split_link(struct bt_conn *conn) {
    struct bt_conn_info info;
    return bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL;
}

/** Discovery starts once the split link is encrypted, since the service requires it. */
static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    if (err || level < BT_SECURITY_L2 || !is_split_link(conn)) {
        return;
    }
    k_mutex_lock(&lock, K_FOREVER);
    if (link_conn != NULL) {
        k_mutex_unlock(&lock);
        return;
    }
    link_conn = bt_conn_ref(conn);
    to_peripheral_handle = 0;
    k_mutex_unlock(&lock);

    memset(&subscribe_params, 0, sizeof(subscribe_params));
    static const struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(PDA_LINK_SERVICE_UUID);
    discover_params = (struct bt_gatt_discover_params){
        .uuid = &service_uuid.uuid,
        .func = service_discovered,
        .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
        .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
        .type = BT_GATT_DISCOVER_PRIMARY,
    };
    const int disc_err = bt_gatt_discover(conn, &discover_params);
    if (disc_err) {
        LOG_ERR("PDA service discovery failed to start: %d", disc_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    k_mutex_lock(&lock, K_FOREVER);
    if (conn != link_conn) {
        k_mutex_unlock(&lock);
        return;
    }
    bt_conn_unref(link_conn);
    link_conn = NULL;
    to_peripheral_handle = 0;
    text_rx_len = 0;
    k_mutex_unlock(&lock);
    atomic_clear(&link_ready);
    LOG_INF("PDA link down (reason 0x%02x)", reason);
    k_work_submit(&leave_work);
    k_work_reschedule(&latency_work, K_NO_WAIT);
}

BT_CONN_CB_DEFINE(roamyboard_pda) = {
    .security_changed = security_changed,
    .disconnected = disconnected,
};
