/*
 * The peripheral's side of the micro PDA: the split data channel's GATT service, and the
 * shell that the forwarded keys drive, with the note kept in settings. docs/pda.md describes
 * it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>

#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include <roamyboard/build_id.h>
#include <roamyboard/chain_state.h>
#include <roamyboard/events/chain_state_changed.h>
#include <roamyboard/events/pda_changed.h>
#include <roamyboard/pda.h>

#include "link.h"
#include "shell.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define SETTINGS_KEY "roamyboard/pda/note"

/** Notifications stay within the smallest ATT MTU (23), whatever the link negotiated. */
#define NOTIFY_MAX 20

static atomic_t mode = ATOMIC_INIT(ROAMYBOARD_PDA_OFF);

enum roamyboard_pda_mode roamyboard_pda_mode(void) {
    return (enum roamyboard_pda_mode)atomic_get(&mode);
}

/** Guards shell, which the system work queue changes and the display work queue draws. */
static K_MUTEX_DEFINE(lock);
static struct pda_shell shell;
static bool shell_ready;

/** Settings may load before anything else touches the shell. Call with lock held. */
static struct pda_shell *locked_shell(void) {
    if (!shell_ready) {
        pda_shell_init(&shell);
        shell_ready = true;
    }
    return &shell;
}

static void changed(void) { raise_roamyboard_pda_changed((struct roamyboard_pda_changed){}); }

void roamyboard_pda_render(uint8_t bits[ROAMYBOARD_PDA_FB_BYTES]) {
    static struct pda_fb fb;
    k_mutex_lock(&lock, K_FOREVER);
    pda_shell_draw(locked_shell(), &fb);
    k_mutex_unlock(&lock);
    memcpy(bits, fb.bits, sizeof(fb.bits));
}

/* The note in settings. System work queue only, except settings_set at load. Static buffers:
 * the system work queue's stack is small, and settings writes go deep. */
static char saved[PDA_NOTE_MAX];
static size_t saved_len;

static void save_note(void) {
    static char text[PDA_NOTE_MAX];
    k_mutex_lock(&lock, K_FOREVER);
    const size_t len = locked_shell()->note.len;
    memcpy(text, shell.note.text, len);
    k_mutex_unlock(&lock);
    if (len == saved_len && memcmp(text, saved, len) == 0) {
        return;
    }
    const int err = len ? settings_save_one(SETTINGS_KEY, text, len) : settings_delete(SETTINGS_KEY);
    if (err) {
        LOG_ERR("Failed to %s setting " SETTINGS_KEY ": %d", len ? "save" : "delete", err);
        return;
    }
    memcpy(saved, text, len);
    saved_len = len;
}

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "note") != 0 || len > PDA_NOTE_MAX) {
        LOG_WRN("Ignoring setting roamyboard/pda/%s of %u bytes: expected note, at most %d bytes",
                name, (unsigned int)len, PDA_NOTE_MAX);
        return 0;
    }
    const ssize_t got = read_cb(cb_arg, saved, len);
    if (got < 0) {
        LOG_ERR("Failed to read setting " SETTINGS_KEY ": %d", (int)got);
        return (int)got;
    }
    saved_len = (size_t)got;
    k_mutex_lock(&lock, K_FOREVER);
    pda_note_set_text(&locked_shell()->note, saved, saved_len);
    k_mutex_unlock(&lock);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(roamyboard_pda, "roamyboard/pda", NULL, settings_set, NULL, NULL);

/* Outgoing: one job at a time, text in chunks and then a final message. Runs on the system
 * work queue and retries when the stack is out of buffers. */
static char out_text[PDA_NOTE_MAX];
static size_t out_len;
static size_t out_pos;
static uint8_t out_final;
static bool out_busy;

static void out_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(out_work, out_handler);

static ssize_t write_to_peripheral(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

BT_GATT_SERVICE_DEFINE(roamyboard_pda_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(PDA_LINK_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PDA_LINK_TO_PERIPHERAL_UUID),
                                              BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT, NULL,
                                              write_to_peripheral, NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(PDA_LINK_TO_CENTRAL_UUID),
                                              BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
                                              NULL),
                       BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

/** The to-central characteristic's value attribute: after the service, two characteristics'
 * declarations and the to-peripheral value. */
#define TO_CENTRAL_ATTR (&roamyboard_pda_svc.attrs[4])

static int notify(const uint8_t *data, size_t len) {
    return bt_gatt_notify(NULL, TO_CENTRAL_ATTR, data, len);
}

static void out_handler(struct k_work *work) {
    while (out_pos < out_len) {
        uint8_t chunk[NOTIFY_MAX];
        const size_t n = MIN(out_len - out_pos, sizeof(chunk) - 1);
        chunk[0] = PDA_LINK_TEXT;
        memcpy(chunk + 1, out_text + out_pos, n);
        const int err = notify(chunk, n + 1);
        if (err == -ENOMEM) {
            k_work_schedule(&out_work, K_MSEC(10));
            return;
        }
        if (err) {
            LOG_ERR("PDA link notify failed at byte %u of %u: %d", (unsigned int)out_pos,
                    (unsigned int)out_len, err);
            out_busy = false;
            return;
        }
        out_pos += n;
    }
    const int err = notify(&out_final, 1);
    if (err == -ENOMEM) {
        k_work_schedule(&out_work, K_MSEC(10));
        return;
    }
    if (err) {
        LOG_ERR("PDA link notify of message 0x%02x failed: %d", out_final, err);
    }
    out_busy = false;
}

/** Sends text, then final. Does nothing while an earlier job is still going out. */
static void send_job(const char *text, size_t len, uint8_t final) {
    if (out_busy) {
        LOG_WRN("PDA link busy; dropped message 0x%02x", final);
        return;
    }
    out_busy = true;
    if (len) {
        memcpy(out_text, text, len);
    }
    out_len = len;
    out_pos = 0;
    out_final = final;
    k_work_schedule(&out_work, K_NO_WAIT);
}

/** Carries out what the shell asked for. System work queue only. */
static void act(enum pda_shell_action action) {
    switch (action) {
    case PDA_SHELL_NONE:
        return;
    case PDA_SHELL_REDRAW:
        break;
    case PDA_SHELL_TYPE_OUT: {
        static char text[PDA_NOTE_MAX];
        k_mutex_lock(&lock, K_FOREVER);
        const size_t len = shell.type_out_len;
        memcpy(text, shell.type_out, len);
        k_mutex_unlock(&lock);
        send_job(text, len, PDA_LINK_TYPE_OUT);
        break;
    }
    case PDA_SHELL_LEAVE:
        send_job(NULL, 0, PDA_LINK_LEAVE);
        break;
    }
    changed();
}

static void set_mode(enum roamyboard_pda_mode new_mode) {
    atomic_set(&mode, new_mode);
    k_mutex_lock(&lock, K_FOREVER);
    const enum pda_shell_action action = pda_shell_set_mode(locked_shell(), new_mode);
    k_mutex_unlock(&lock);
    if (new_mode == ROAMYBOARD_PDA_OFF) {
        save_note();
    }
    act(action);
}

static void set_left_info(const uint8_t *data, size_t len) {
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_half_info *left = &locked_shell()->left;
    left->known = true;
    left->key_module_count = (int8_t)data[1];
    left->battery = data[2];
    const size_t id_len = MIN(len - 3, sizeof(left->build_id) - 1);
    memcpy(left->build_id, data + 3, id_len);
    left->build_id[id_len] = '\0';
    k_mutex_unlock(&lock);
    changed();
}

static void update_right_info(void) {
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_half_info *right = &locked_shell()->right;
    right->known = true;
    right->key_module_count = roamyboard_chain_key_module_count();
    right->battery = zmk_battery_state_of_charge();
    strncpy(right->build_id, roamyboard_build_id(), sizeof(right->build_id) - 1);
    k_mutex_unlock(&lock);
    changed();
}

/* Incoming: the write callback runs on the Bluetooth receive thread and queues; the rest runs
 * on the system work queue. */
struct incoming {
    uint8_t len;
    uint8_t data[PDA_LINK_WRITE_MAX];
};
K_MSGQ_DEFINE(incoming_msgq, sizeof(struct incoming), 32, 1);

static void incoming_handler(struct k_work *work) {
    struct incoming msg;
    while (k_msgq_get(&incoming_msgq, &msg, K_NO_WAIT) == 0) {
        switch (msg.data[0]) {
        case PDA_LINK_MODE:
            if (msg.len == 2 && msg.data[1] <= ROAMYBOARD_PDA_NOTE) {
                set_mode((enum roamyboard_pda_mode)msg.data[1]);
                continue;
            }
            break;
        case PDA_LINK_KEY:
            if (msg.len == PDA_LINK_KEY_LEN) {
                k_mutex_lock(&lock, K_FOREVER);
                const enum pda_shell_action action =
                    pda_shell_key(locked_shell(), msg.data[3], msg.data[2], msg.data[1]);
                k_mutex_unlock(&lock);
                act(action);
                continue;
            }
            break;
        case PDA_LINK_POSITION:
            if (msg.len == PDA_LINK_POSITION_LEN) {
                k_mutex_lock(&lock, K_FOREVER);
                const enum pda_shell_action action =
                    pda_shell_position(locked_shell(), msg.data[2], msg.data[1]);
                k_mutex_unlock(&lock);
                act(action);
                continue;
            }
            break;
        case PDA_LINK_INFO:
            if (msg.len >= 3) {
                set_left_info(msg.data, msg.len);
                continue;
            }
            break;
        }
        LOG_WRN("PDA link: bad message 0x%02x of %u bytes", msg.data[0], msg.len);
    }
}

static K_WORK_DEFINE(incoming_work, incoming_handler);

static ssize_t write_to_peripheral(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len == 0 || len > PDA_LINK_WRITE_MAX) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    struct incoming msg = {.len = (uint8_t)len};
    memcpy(msg.data, buf, len);
    if (k_msgq_put(&incoming_msgq, &msg, K_NO_WAIT) != 0) {
        LOG_WRN("PDA link queue full; dropped message 0x%02x", msg.data[0]);
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }
    k_work_submit(&incoming_work);
    return len;
}

static int state_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *split =
        as_zmk_split_peripheral_status_changed(eh);
    if (split != NULL) {
        // Without the central, nothing can leave PDA mode, so losing it leaves.
        if (!split->connected) {
            set_mode(ROAMYBOARD_PDA_OFF);
        }
        // The display shows the PDA only while the central is connected.
        changed();
        return ZMK_EV_EVENT_BUBBLE;
    }
    update_right_info();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_pda, state_listener);
ZMK_SUBSCRIPTION(roamyboard_pda, zmk_split_peripheral_status_changed);
ZMK_SUBSCRIPTION(roamyboard_pda, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(roamyboard_pda, roamyboard_chain_state_changed);
