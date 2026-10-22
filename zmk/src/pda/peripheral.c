/*
 * The peripheral's side of the micro PDA: the split data channel's GATT service, and the
 * shell that the forwarded keys drive, with the note kept in settings. docs/pda.md describes
 * it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

/** The single note of builds before the document list; it becomes a document at load. */
#define LEGACY_NOTE_KEY "roamyboard/pda/note"
/** Followed by the slot: created and modified stamps (uint32 each, little-endian), then text. */
#define DOC_KEY "roamyboard/pda/doc/"
#define DOC_HEADER 8
#define DOC_SORT_KEY "roamyboard/pda/docsort"
#define SCORES_KEY "roamyboard/pda/scores"
#define KANA_KEY "roamyboard/pda/kana"
#define VIEW_KEY "roamyboard/pda/app"
#define CAT_KEY "roamyboard/pda/cat"
#define VIEW_NAME_MAX 16

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

/**
 * The shell, with now_ms set; settings may load before anything else touches it. Call with
 * lock held.
 */
static struct pda_shell *locked_shell(void) {
    if (!shell_ready) {
        pda_shell_init(&shell, k_cycle_get_32());
        shell_ready = true;
    }
    shell.now_ms = k_uptime_get_32();
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

/* The Note app's documents in settings. System work queue only, except at load. Static
 * buffers: the system work queue's stack is small, and settings writes go deep. */
static uint8_t doc_value[DOC_HEADER + PDA_NOTE_MAX];
static char legacy_note[PDA_NOTE_MAX];
static size_t legacy_len;
/** Settings still hold LEGACY_NOTE_KEY. */
static bool legacy_present;

static void doc_key_for(char *key, size_t size, int slot) { snprintf(key, size, DOC_KEY "%d", slot); }

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/** Writes the documents and the sort that differ from settings. */
static void save_docs(void) {
    char key[sizeof(DOC_KEY) + 4];
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_docs *docs = &locked_shell()->docs;
    uint16_t dirty = docs->dirty, gone = docs->gone;
    const bool sort_changed = docs->sort_changed;
    const uint8_t sort = docs->sort;
    docs->dirty = docs->gone = 0;
    docs->sort_changed = false;
    k_mutex_unlock(&lock);

    uint16_t failed = 0;
    for (int slot = 0; slot < PDA_DOCS_MAX; slot++) {
        const uint16_t bit = (uint16_t)(1u << slot);
        doc_key_for(key, sizeof(key), slot);
        if (gone & bit) {
            const int err = settings_delete(key);
            if (err) {
                LOG_ERR("Failed to delete setting %s: %d", key, err);
                failed |= bit;
            }
        }
        if (!(dirty & bit)) {
            continue;
        }
        k_mutex_lock(&lock, K_FOREVER);
        const struct pda_doc *doc = &shell.docs.docs[slot];
        const size_t len = doc->note.len;
        put_u32(doc_value, doc->created);
        put_u32(doc_value + 4, doc->modified);
        memcpy(doc_value + DOC_HEADER, doc->note.text, len);
        k_mutex_unlock(&lock);
        const int err = settings_save_one(key, doc_value, DOC_HEADER + len);
        if (err) {
            LOG_ERR("Failed to save setting %s of %u bytes: %d", key,
                    (unsigned int)(DOC_HEADER + len), err);
            failed |= bit;
        }
    }
    bool sort_failed = false;
    if (sort_changed) {
        const int err = settings_save_one(DOC_SORT_KEY, &sort, 1);
        if (err) {
            LOG_ERR("Failed to save setting " DOC_SORT_KEY ": %d", err);
            sort_failed = true;
        }
    }
    if (failed || sort_failed) {
        // Try again at the next save: failed slots count as dirty and gone both, which
        // rewrites a document that exists and deletes one that does not.
        k_mutex_lock(&lock, K_FOREVER);
        for (int slot = 0; slot < PDA_DOCS_MAX; slot++) {
            const uint16_t bit = (uint16_t)(1u << slot);
            if (failed & bit) {
                *(shell.docs.docs[slot].used ? &shell.docs.dirty : &shell.docs.gone) |= bit;
            }
        }
        shell.docs.sort_changed |= sort_failed;
        k_mutex_unlock(&lock);
        return;
    }
    if (legacy_present) {
        const int err = settings_delete(LEGACY_NOTE_KEY);
        if (err) {
            LOG_ERR("Failed to delete setting " LEGACY_NOTE_KEY ": %d", err);
        } else {
            legacy_present = false;
        }
    }
}

/** Writes the best scores if a game changed them. System work queue only. */
static void save_scores(void) {
    struct pda_scores scores;
    k_mutex_lock(&lock, K_FOREVER);
    const bool changed_scores = locked_shell()->scores_changed;
    scores = shell.scores;
    shell.scores_changed = false;
    k_mutex_unlock(&lock);
    if (!changed_scores) {
        return;
    }
    const int err = settings_save_one(SCORES_KEY, &scores, sizeof(scores));
    if (err) {
        LOG_ERR("Failed to save setting " SCORES_KEY ": %d", err);
        k_mutex_lock(&lock, K_FOREVER);
        shell.scores_changed = true;
        k_mutex_unlock(&lock);
    }
}

/** The view in settings, which opens again at startup. System work queue only, except at load. */
static char saved_view[VIEW_NAME_MAX];

/** Writes the view shown when it differs from settings, so that it outlives a power-off. */
static void save_view(void) {
    char view[VIEW_NAME_MAX];
    k_mutex_lock(&lock, K_FOREVER);
    strncpy(view, pda_shell_view_name(locked_shell()), sizeof(view) - 1);
    view[sizeof(view) - 1] = '\0';
    k_mutex_unlock(&lock);
    if (strcmp(view, saved_view) == 0) {
        return;
    }
    const int err = settings_save_one(VIEW_KEY, view, strlen(view));
    if (err) {
        LOG_ERR("Failed to save setting " VIEW_KEY ": %d", err);
        return;
    }
    strcpy(saved_view, view);
}

/** Writes the cat's needs if they changed by enough. System work queue only. */
static void save_cat(void) {
    k_mutex_lock(&lock, K_FOREVER);
    const bool changed_cat = locked_shell()->cat_changed;
    const struct pda_cat_saved needs = shell.cat.needs;
    shell.cat_changed = false;
    k_mutex_unlock(&lock);
    if (!changed_cat) {
        return;
    }
    const int err = settings_save_one(CAT_KEY, &needs, sizeof(needs));
    if (err) {
        LOG_ERR("Failed to save setting " CAT_KEY ": %d", err);
        k_mutex_lock(&lock, K_FOREVER);
        shell.cat_changed = true;
        k_mutex_unlock(&lock);
    }
}

/** Writes the Kana drill's weights and counts if they changed. System work queue only. */
static void save_kana(void) {
    static uint8_t blob[PDA_KANA_SAVED];
    k_mutex_lock(&lock, K_FOREVER);
    const bool changed_kana = locked_shell()->kana_changed;
    memcpy(blob, &shell.kana, sizeof(blob));
    shell.kana_changed = false;
    k_mutex_unlock(&lock);
    if (!changed_kana) {
        return;
    }
    const int err = settings_save_one(KANA_KEY, blob, sizeof(blob));
    if (err) {
        LOG_ERR("Failed to save setting " KANA_KEY ": %d", err);
        k_mutex_lock(&lock, K_FOREVER);
        shell.kana_changed = true;
        k_mutex_unlock(&lock);
    }
}

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "app") == 0) {
        if (len >= sizeof(saved_view)) {
            LOG_WRN("Ignoring setting " VIEW_KEY " of %u bytes: expected fewer than %u",
                    (unsigned int)len, (unsigned int)sizeof(saved_view));
            return 0;
        }
        const ssize_t got = read_cb(cb_arg, saved_view, len);
        if (got != (ssize_t)len) {
            LOG_ERR("Failed to read setting " VIEW_KEY ": %d", (int)got);
            saved_view[0] = '\0';
            return got < 0 ? (int)got : -EIO;
        }
        saved_view[len] = '\0';
        k_mutex_lock(&lock, K_FOREVER);
        const bool shown = pda_shell_show(locked_shell(), saved_view);
        k_mutex_unlock(&lock);
        if (!shown) {
            LOG_WRN("Setting " VIEW_KEY " names no app: \"%s\"", saved_view);
        }
        return 0;
    }
    if (strcmp(name, "kana") == 0) {
        static uint8_t blob[PDA_KANA_SAVED];
        if (len != sizeof(blob)) {
            LOG_WRN("Ignoring setting " KANA_KEY " of %u bytes: expected %u", (unsigned int)len,
                    (unsigned int)sizeof(blob));
            return 0;
        }
        const ssize_t got = read_cb(cb_arg, blob, len);
        if (got != (ssize_t)len) {
            LOG_ERR("Failed to read setting " KANA_KEY ": %d", (int)got);
            return got < 0 ? (int)got : -EIO;
        }
        k_mutex_lock(&lock, K_FOREVER);
        memcpy(&locked_shell()->kana, blob, sizeof(blob));
        k_mutex_unlock(&lock);
        return 0;
    }
    if (strcmp(name, "cat") == 0) {
        // Like the scores, the needs only gain fields at the end.
        struct pda_cat_saved needs;
        k_mutex_lock(&lock, K_FOREVER);
        needs = locked_shell()->cat.needs;
        k_mutex_unlock(&lock);
        if (len > sizeof(needs)) {
            LOG_WRN("Ignoring setting " CAT_KEY " of %u bytes: expected at most %u",
                    (unsigned int)len, (unsigned int)sizeof(needs));
            return 0;
        }
        const ssize_t got = read_cb(cb_arg, &needs, len);
        if (got != (ssize_t)len) {
            LOG_ERR("Failed to read setting " CAT_KEY ": %d", (int)got);
            return got < 0 ? (int)got : -EIO;
        }
        k_mutex_lock(&lock, K_FOREVER);
        shell.cat.needs = needs;
        k_mutex_unlock(&lock);
        return 0;
    }
    if (strcmp(name, "scores") == 0) {
        // Scores only gain fields at the end: an older, shorter value leaves the new ones 0.
        struct pda_scores scores = {0};
        if (len > sizeof(scores)) {
            LOG_WRN("Ignoring setting " SCORES_KEY " of %u bytes: expected at most %u",
                    (unsigned int)len, (unsigned int)sizeof(scores));
            return 0;
        }
        const ssize_t got = read_cb(cb_arg, &scores, len);
        if (got != (ssize_t)len) {
            LOG_ERR("Failed to read setting " SCORES_KEY ": %d", (int)got);
            return got < 0 ? (int)got : -EIO;
        }
        k_mutex_lock(&lock, K_FOREVER);
        locked_shell()->scores = scores;
        k_mutex_unlock(&lock);
        return 0;
    }
    if (strncmp(name, "doc/", 4) == 0) {
        const int slot = atoi(name + 4);
        if (slot < 0 || slot >= PDA_DOCS_MAX || len < DOC_HEADER ||
            len > DOC_HEADER + PDA_NOTE_MAX) {
            LOG_WRN("Ignoring setting roamyboard/pda/%s of %u bytes: expected a slot below %d "
                    "and %d to %d bytes",
                    name, (unsigned int)len, PDA_DOCS_MAX, DOC_HEADER, DOC_HEADER + PDA_NOTE_MAX);
            return 0;
        }
        const ssize_t got = read_cb(cb_arg, doc_value, len);
        if (got != (ssize_t)len) {
            LOG_ERR("Failed to read setting roamyboard/pda/%s: %d", name, (int)got);
            return got < 0 ? (int)got : -EIO;
        }
        k_mutex_lock(&lock, K_FOREVER);
        pda_docs_load(&locked_shell()->docs, slot, get_u32(doc_value), get_u32(doc_value + 4),
                      (const char *)doc_value + DOC_HEADER, len - DOC_HEADER);
        k_mutex_unlock(&lock);
        return 0;
    }
    if (strcmp(name, "docsort") == 0) {
        uint8_t sort;
        if (len != 1 || read_cb(cb_arg, &sort, 1) != 1 || sort >= PDA_DOCS_SORT_COUNT) {
            LOG_WRN("Ignoring setting " DOC_SORT_KEY " of %u bytes", (unsigned int)len);
            return 0;
        }
        k_mutex_lock(&lock, K_FOREVER);
        locked_shell()->docs.sort = sort;
        k_mutex_unlock(&lock);
        return 0;
    }
    if (strcmp(name, "note") != 0 || len > PDA_NOTE_MAX) {
        LOG_WRN("Ignoring setting roamyboard/pda/%s of %u bytes", name, (unsigned int)len);
        return 0;
    }
    const ssize_t got = read_cb(cb_arg, legacy_note, len);
    if (got < 0) {
        LOG_ERR("Failed to read setting " LEGACY_NOTE_KEY ": %d", (int)got);
        return (int)got;
    }
    legacy_len = (size_t)got;
    legacy_present = true;
    return 0;
}

/** After loading: the old single note becomes the first document, unless documents exist. */
static int settings_commit(void) {
    if (!legacy_present || legacy_len == 0) {
        return 0;
    }
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_docs *docs = &locked_shell()->docs;
    if (pda_docs_count(docs) == 0) {
        const int slot = pda_docs_create(docs);
        pda_note_set_text(&docs->docs[slot].note, legacy_note, legacy_len);
        LOG_INF("Moved setting " LEGACY_NOTE_KEY " to document %d", slot);
    }
    k_mutex_unlock(&lock);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(roamyboard_pda, "roamyboard/pda", NULL, settings_set,
                               settings_commit, NULL);

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

/* The cat's whereabouts for the central's status screen: only the latest state matters. */
static bool cat_sent_valid;
static bool cat_sent_here;

static void cat_out_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cat_out_work, cat_out_handler);

static void cat_out_handler(struct k_work *work) {
    k_mutex_lock(&lock, K_FOREVER);
    const bool here = pda_shell_cat_here(locked_shell());
    k_mutex_unlock(&lock);
    const uint8_t msg[] = {PDA_LINK_CAT, here};
    const int err = notify(msg, sizeof(msg));
    if (err == -ENOMEM) {
        k_work_schedule(&cat_out_work, K_MSEC(10));
        return;
    }
    if (err) {
        // The central asks again with PDA_LINK_INFO when the link comes up.
        LOG_DBG("PDA link notify of the cat failed: %d", err);
        cat_sent_valid = false;
        return;
    }
    cat_sent_valid = true;
    cat_sent_here = here;
}

/** Tells the central where the cat is if that changed, or always if force. System work queue. */
static void sync_cat(bool force) {
    k_mutex_lock(&lock, K_FOREVER);
    const bool here = pda_shell_cat_here(locked_shell());
    k_mutex_unlock(&lock);
    if (force || !cat_sent_valid || here != cat_sent_here) {
        k_work_reschedule(&cat_out_work, K_NO_WAIT);
    }
}

static void app_tick_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(app_tick_work, app_tick_handler);

/** Follows the open app's tick_ms: ticks while it asks for them, and stops when it stops. */
static void follow_tick(void) {
    k_mutex_lock(&lock, K_FOREVER);
    const uint32_t interval = pda_shell_tick_ms(locked_shell());
    k_mutex_unlock(&lock);
    if (interval == 0) {
        k_work_cancel_delayable(&app_tick_work);
    } else if (!(k_work_delayable_busy_get(&app_tick_work) & (K_WORK_DELAYED | K_WORK_QUEUED))) {
        // Not k_work_delayable_is_pending(): that is true while the tick itself runs, and the
        // tick calls this to schedule the next one.
        k_work_schedule(&app_tick_work, K_MSEC(interval));
    }
}

/** Carries out what the shell asked for, then follows its tick. System work queue only. */
static void act(enum pda_shell_action action) {
    follow_tick();
    sync_cat(false);
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

static void app_tick_handler(struct k_work *work) {
    k_mutex_lock(&lock, K_FOREVER);
    const enum pda_shell_action action = pda_shell_tick(locked_shell());
    k_mutex_unlock(&lock);
    save_cat();
    save_view();
    act(action);
}

static void set_mode(enum roamyboard_pda_mode new_mode) {
    atomic_set(&mode, new_mode);
    k_mutex_lock(&lock, K_FOREVER);
    const enum pda_shell_action action = pda_shell_set_mode(locked_shell(), new_mode);
    k_mutex_unlock(&lock);
    if (new_mode == ROAMYBOARD_PDA_OFF) {
        save_docs();
        save_scores();
        save_kana();
        save_cat();
    }
    save_view();
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

static void tick_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tick_work, tick_handler);

/** Redraws at each minute's start while the clock is set. */
static void tick_handler(struct k_work *work) {
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_shell *s = locked_shell();
    s->now_ms = k_uptime_get_32();
    struct pda_date_time now;
    const bool set = pda_shell_time(s, &now);
    k_mutex_unlock(&lock);
    if (set) {
        k_work_reschedule(&tick_work, K_SECONDS(60 - now.second));
        changed();
    }
}

static void set_clock(const uint8_t *data) {
    k_mutex_lock(&lock, K_FOREVER);
    struct pda_clock_state *clock = &locked_shell()->clock;
    clock->status = (enum pda_clock_status)data[1];
    clock->profile = data[2];
    clock->seconds = (uint32_t)data[3] | (uint32_t)data[4] << 8 | (uint32_t)data[5] << 16 |
                     (uint32_t)data[6] << 24;
    clock->at_ms = k_uptime_get_32();
    k_mutex_unlock(&lock);
    k_work_reschedule(&tick_work, K_NO_WAIT);
    changed();
}

static void set_midi(const uint8_t *data) {
    struct pda_midi_view view = {
        .shown = data[1],
        .octave = (int8_t)data[2],
        .transpose = (int8_t)data[3],
        .velocity_step = data[4],
        .channel = data[5],
        .last_note = data[6] == 0xFF ? -1 : data[6],
    };
    for (int i = 0; i < (PDA_LINK_MIDI_LEN - 7) * 8; i++) {
        if (data[7 + i / 8] & (1 << (i % 8))) {
            const int note = PDA_LINK_MIDI_FIRST_NOTE + i;
            view.held[note / 8] |= 1 << (note % 8);
        }
    }
    k_mutex_lock(&lock, K_FOREVER);
    const enum pda_shell_action action = pda_shell_midi(locked_shell(), &view);
    k_mutex_unlock(&lock);
    act(action);
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
                struct pda_shell *s = locked_shell();
                // The moments of key presses keep the games' random numbers unpredictable.
                s->random_state ^= k_cycle_get_32();
                if (s->random_state == 0) {
                    s->random_state = 1;
                }
                const enum pda_shell_action action =
                    pda_shell_key(s, msg.data[3], msg.data[2], msg.data[1]);
                k_mutex_unlock(&lock);
                save_view();
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
        case PDA_LINK_TIME:
            if (msg.len == PDA_LINK_TIME_LEN && msg.data[1] <= PDA_CLOCK_ERROR) {
                set_clock(msg.data);
                continue;
            }
            break;
        case PDA_LINK_MIDI:
            if (msg.len == PDA_LINK_MIDI_LEN) {
                set_midi(msg.data);
                continue;
            }
            break;
        case PDA_LINK_INFO:
            if (msg.len >= 3) {
                set_left_info(msg.data, msg.len);
                sync_cat(true);
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

/** Without the central, nothing can leave PDA mode, so losing it leaves. */
static void central_lost_handler(struct k_work *work) {
    set_mode(ROAMYBOARD_PDA_OFF);
    k_mutex_lock(&lock, K_FOREVER);
    locked_shell()->midi.shown = false;
    k_mutex_unlock(&lock);
}
static K_WORK_DEFINE(central_lost_work, central_lost_handler);

/** The PDA shows again: the app that it shows, perhaps restored at startup, ticks again. */
static void central_found_handler(struct k_work *work) { act(PDA_SHELL_REDRAW); }
static K_WORK_DEFINE(central_found_work, central_found_handler);

static int state_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *split =
        as_zmk_split_peripheral_status_changed(eh);
    if (split != NULL) {
        // ZMK raises this on the Bluetooth thread; settings writes belong on the work queue.
        k_work_submit(split->connected ? &central_found_work : &central_lost_work);
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
