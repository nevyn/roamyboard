/*
 * The peripheral's side of the micro PDA: the split data channel's GATT service, and the note
 * that the forwarded keys edit, kept in settings. docs/pda.md describes it.
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

#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include <roamyboard/events/pda_changed.h>
#include <roamyboard/pda.h>

#include "link.h"
#include "note.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(PDA_NOTE_MAX == ROAMYBOARD_PDA_NOTE_MAX, "one note limit for the PDA and its screen");

#define SETTINGS_KEY "roamyboard/pda/note"

/** Notifications stay within the smallest ATT MTU (23), whatever the link negotiated. */
#define NOTIFY_MAX 20

static atomic_t mode = ATOMIC_INIT(ROAMYBOARD_PDA_OFF);

enum roamyboard_pda_mode roamyboard_pda_mode(void) {
    return (enum roamyboard_pda_mode)atomic_get(&mode);
}

/** Guards note, which the display work queue reads. */
static K_MUTEX_DEFINE(note_lock);
static struct pda_note note;
/** The note differs from settings. System work queue only. */
static bool unsaved;

size_t roamyboard_pda_note_copy(char *buf, size_t size) {
    k_mutex_lock(&note_lock, K_FOREVER);
    const size_t len = note.len;
    const size_t n = MIN(len, size - 1);
    memcpy(buf, note.text, n);
    buf[n] = '\0';
    k_mutex_unlock(&note_lock);
    return len;
}

static void save_note(void) {
    if (!unsaved) {
        return;
    }
    // Static: the system work queue's stack is small, and settings writes go deep.
    static char text[PDA_NOTE_MAX + 1];
    const size_t len = roamyboard_pda_note_copy(text, sizeof(text));
    const int err = len ? settings_save_one(SETTINGS_KEY, text, len) : settings_delete(SETTINGS_KEY);
    if (err) {
        LOG_ERR("Failed to %s setting " SETTINGS_KEY ": %d", len ? "save" : "delete", err);
        return;
    }
    unsaved = false;
}

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "note") != 0 || len > PDA_NOTE_MAX) {
        LOG_WRN("Ignoring setting roamyboard/pda/%s of %u bytes: expected note, at most %d bytes",
                name, (unsigned int)len, PDA_NOTE_MAX);
        return 0;
    }
    static char buf[PDA_NOTE_MAX];
    const ssize_t got = read_cb(cb_arg, buf, len);
    if (got < 0) {
        LOG_ERR("Failed to read setting " SETTINGS_KEY ": %d", (int)got);
        return (int)got;
    }
    k_mutex_lock(&note_lock, K_FOREVER);
    pda_note_set_text(&note, buf, (size_t)got);
    k_mutex_unlock(&note_lock);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(roamyboard_pda, "roamyboard/pda", NULL, settings_set, NULL, NULL);

/* Outgoing: one job at a time, the note's text in chunks and then a final message. Runs on
 * the system work queue and retries when the stack is out of buffers. */
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

/* Incoming: the write callback runs on the Bluetooth receive thread and queues; the rest runs
 * on the system work queue. */
struct incoming {
    uint8_t len;
    uint8_t data[PDA_LINK_KEY_LEN];
};
K_MSGQ_DEFINE(incoming_msgq, sizeof(struct incoming), 32, 1);

static void set_mode(enum roamyboard_pda_mode new_mode) {
    k_mutex_lock(&note_lock, K_FOREVER);
    pda_note_reset_keys(&note);
    k_mutex_unlock(&note_lock);
    if (atomic_set(&mode, new_mode) == new_mode) {
        return;
    }
    if (new_mode == ROAMYBOARD_PDA_OFF) {
        save_note();
    }
    raise_roamyboard_pda_changed((struct roamyboard_pda_changed){});
}

static void apply_key(bool pressed, uint8_t implicit_mods, uint8_t usage) {
    if (roamyboard_pda_mode() == ROAMYBOARD_PDA_OFF) {
        return;
    }
    static char text[PDA_NOTE_MAX];
    size_t len = 0;
    k_mutex_lock(&note_lock, K_FOREVER);
    const enum pda_note_action action = pda_note_key(&note, usage, implicit_mods, pressed);
    if (action == PDA_NOTE_TYPE_OUT) {
        len = note.len;
        memcpy(text, note.text, len);
        pda_note_clear(&note);
    }
    k_mutex_unlock(&note_lock);

    switch (action) {
    case PDA_NOTE_NONE:
        break;
    case PDA_NOTE_CHANGED:
        unsaved = true;
        raise_roamyboard_pda_changed((struct roamyboard_pda_changed){});
        break;
    case PDA_NOTE_TYPE_OUT:
        unsaved = true;
        send_job(text, len, PDA_LINK_TYPE_OUT);
        raise_roamyboard_pda_changed((struct roamyboard_pda_changed){});
        break;
    case PDA_NOTE_LEAVE:
        send_job(NULL, 0, PDA_LINK_LEAVE);
        break;
    }
}

static void incoming_handler(struct k_work *work) {
    struct incoming msg;
    while (k_msgq_get(&incoming_msgq, &msg, K_NO_WAIT) == 0) {
        switch (msg.data[0]) {
        case PDA_LINK_MODE:
            if (msg.len == 2 && msg.data[1] <= ROAMYBOARD_PDA_NOTE) {
                set_mode((enum roamyboard_pda_mode)msg.data[1]);
                break;
            }
            LOG_WRN("PDA link: bad mode message of %u bytes, mode %u", msg.len, msg.data[1]);
            break;
        case PDA_LINK_KEY:
            if (msg.len == PDA_LINK_KEY_LEN) {
                apply_key(msg.data[1], msg.data[2], msg.data[3]);
                break;
            }
            LOG_WRN("PDA link: key message of %u bytes, expected %d", msg.len, PDA_LINK_KEY_LEN);
            break;
        default:
            LOG_WRN("PDA link: unknown message 0x%02x of %u bytes", msg.data[0], msg.len);
            break;
        }
    }
}

static K_WORK_DEFINE(incoming_work, incoming_handler);

static ssize_t write_to_peripheral(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len == 0 || len > PDA_LINK_KEY_LEN) {
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

/** Without the central, nothing can leave PDA mode, so losing it leaves. */
static int split_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (ev != NULL && !ev->connected) {
        set_mode(ROAMYBOARD_PDA_OFF);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_pda, split_listener);
ZMK_SUBSCRIPTION(roamyboard_pda, zmk_split_peripheral_status_changed);
