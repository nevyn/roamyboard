/*
 * The relay of the right half's records, on the split central: a GATT client for the right
 * half's log service over the split connection. It starts and stops the right half's session
 * with the host's, forwards the host's level and list commands, and hands each record that
 * arrives to the left half's session. docs/logging.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/atomic.h>

#include "command.h"
#include "internal.h"

/* lock guards link_conn, command_handle and the resume point; the Bluetooth receive thread
 * and the log service's work queue take it. */
static struct k_spinlock lock;
static struct bt_conn *link_conn;
static uint16_t command_handle;
/** The sequence number after the right half's latest record, once one arrived this session. */
static bool have_resume;
static uint32_t resume_seq;
static atomic_t link_ready;

static struct bt_gatt_exchange_params mtu_params;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_discover_params ccc_discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static uint16_t service_end_handle;

/* The host's session, on the log service's work queue. */
static bool session_on;
static char start_cmd[LOG_CMD_MAX];
static size_t start_len;
/** The session's latest level commands, which the right half gets again when it reconnects. */
#define LEVELS_KEPT 8
static struct {
    uint8_t len;
    char text[LOG_CMD_MAX];
} levels[LEVELS_KEPT];
static size_t level_count;

/** Problems on the receive thread, reported from the work queue. */
static atomic_t bad_fragments;
static atomic_t incomplete_records;
static atomic_t link_failure;

static void write_command(const char *cmd, size_t len) {
    struct bt_conn *conn = NULL;
    uint16_t handle = 0;
    K_SPINLOCK(&lock) {
        if (link_conn && atomic_get(&link_ready)) {
            conn = bt_conn_ref(link_conn);
            handle = command_handle;
        }
    }
    if (conn == NULL) {
        return;
    }
    const int err = bt_gatt_write_without_response(conn, handle, cmd, (uint16_t)len, false);
    bt_conn_unref(conn);
    if (err) {
        roamy_log_report(LOG_LEVEL_WRN, "relay: command to the right half failed: %d", err);
    }
}

/** Starts the right half's session where the records that this one already has end. */
static void send_start(void) {
    bool resume;
    uint32_t seq;
    K_SPINLOCK(&lock) {
        resume = have_resume;
        seq = resume_seq;
    }
    if (resume) {
        char cmd[LOG_CMD_MAX];
        const int len = snprintf(cmd, sizeof(cmd), "start R:%u", seq);
        write_command(cmd, (size_t)len);
    } else {
        write_command(start_cmd, start_len);
    }
}

void roamy_log_relay_start(const char *cmd, size_t len) {
    session_on = true;
    level_count = 0;
    memcpy(start_cmd, cmd, MIN(len, sizeof(start_cmd)));
    start_len = MIN(len, sizeof(start_cmd));
    K_SPINLOCK(&lock) { have_resume = false; }
    write_command(start_cmd, start_len);
}

void roamy_log_relay_stop(void) {
    session_on = false;
    write_command("stop", 4);
}

void roamy_log_relay_forward(const char *cmd, size_t len) {
    struct log_cmd parsed;
    if (session_on && log_cmd_parse(cmd, len, &parsed) && parsed.kind == LOG_CMD_LEVEL) {
        if (level_count == LEVELS_KEPT) {
            memmove(&levels[0], &levels[1], sizeof(levels[0]) * (LEVELS_KEPT - 1));
            level_count--;
        }
        levels[level_count].len = (uint8_t)MIN(len, sizeof(levels[0].text));
        memcpy(levels[level_count].text, cmd, levels[level_count].len);
        level_count++;
    }
    write_command(cmd, len);
}

static void problems_handler(struct k_work *work) {
    const atomic_val_t failure = atomic_set(&link_failure, 0);
    if (failure) {
        roamy_log_report(LOG_LEVEL_WRN, "relay: the right half's log service %s (%d)",
                       failure == -ENOENT ? "is missing; is its firmware older?"
                                          : "could not be set up",
                       (int)failure);
    }
    const atomic_val_t bad = atomic_set(&bad_fragments, 0);
    const atomic_val_t incomplete = atomic_set(&incomplete_records, 0);
    if (bad || incomplete) {
        roamy_log_report(LOG_LEVEL_WRN, "relay: dropped %d bad fragments and %d incomplete records "
                                      "from the right half",
                       (int)bad, (int)incomplete);
    }
}

static K_WORK_DEFINE(problems_work, problems_handler);

static void ready_handler(struct k_work *work) {
    if (!atomic_get(&link_ready) || !session_on) {
        return;
    }
    send_start();
    for (size_t i = 0; i < level_count; i++) {
        write_command(levels[i].text, levels[i].len);
    }
}

static K_WORK_DEFINE(ready_work, ready_handler);

static void link_failed(int err) {
    atomic_set(&link_failure, err);
    roamy_log_submit(&problems_work);
}

/* GATT client, on the Bluetooth receive thread. */

static struct log_frame_reassembler reassembler;

static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length) {
    if (data == NULL) {
        atomic_clear(&link_ready);
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }
    const uint32_t incomplete = reassembler.incomplete;
    const enum log_frame_result result = log_frame_reassemble(&reassembler, data, length);
    if (reassembler.incomplete != incomplete) {
        atomic_inc(&incomplete_records);
        roamy_log_submit(&problems_work);
    }
    if (result == LOG_FRAME_COMPLETE) {
        K_SPINLOCK(&lock) {
            have_resume = true;
            resume_seq = reassembler.seq + 1;
        }
        roamy_log_relay_record(reassembler.half, reassembler.backlog, reassembler.seq,
                               reassembler.body, reassembler.len);
    } else if (result != LOG_FRAME_PARTIAL) {
        atomic_inc(&bad_fragments);
        roamy_log_submit(&problems_work);
    }
    return BT_GATT_ITER_CONTINUE;
}

static void subscribed(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params) {
    if (err) {
        link_failed(-EACCES);
        return;
    }
    atomic_set(&link_ready, true);
    roamy_log_submit(&ready_work);
}

static uint8_t chrc_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        uint16_t handle;
        K_SPINLOCK(&lock) { handle = command_handle; }
        if (handle == 0 || subscribe_params.value_handle == 0) {
            link_failed(-ENOENT);
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
            link_failed(err);
        }
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (bt_uuid_cmp(chrc->uuid, BT_UUID_DECLARE_128(ROAMY_LOG_COMMAND_UUID)) == 0) {
        K_SPINLOCK(&lock) { command_handle = chrc->value_handle; }
    } else if (bt_uuid_cmp(chrc->uuid, BT_UUID_DECLARE_128(ROAMY_LOG_RECORDS_UUID)) == 0) {
        subscribe_params.value_handle = chrc->value_handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t service_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        link_failed(-ENOENT);
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
        link_failed(err);
    }
    return BT_GATT_ITER_STOP;
}

static void mtu_exchanged(struct bt_conn *conn, uint8_t err,
                          struct bt_gatt_exchange_params *params) {}

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
    bool fresh = false;
    K_SPINLOCK(&lock) {
        if (link_conn == NULL) {
            link_conn = bt_conn_ref(conn);
            command_handle = 0;
            fresh = true;
        }
    }
    if (!fresh) {
        return;
    }
    log_frame_reassembler_init(&reassembler);
    memset(&subscribe_params, 0, sizeof(subscribe_params));

    // A larger ATT MTU carries a record in fewer notifications; ZMK leaves it at 23.
    mtu_params.func = mtu_exchanged;
    (void)bt_gatt_exchange_mtu(conn, &mtu_params);

    static const struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(ROAMY_LOG_SERVICE_UUID);
    discover_params = (struct bt_gatt_discover_params){
        .uuid = &service_uuid.uuid,
        .func = service_discovered,
        .start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
        .end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
        .type = BT_GATT_DISCOVER_PRIMARY,
    };
    const int disc_err = bt_gatt_discover(conn, &discover_params);
    if (disc_err) {
        link_failed(disc_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    bool ours = false;
    K_SPINLOCK(&lock) {
        if (conn == link_conn) {
            bt_conn_unref(link_conn);
            link_conn = NULL;
            command_handle = 0;
            ours = true;
        }
    }
    if (ours) {
        atomic_clear(&link_ready);
    }
}

BT_CONN_CB_DEFINE(roamyboard_log_relay) = {
    .security_changed = security_changed,
    .disconnected = disconnected,
};
