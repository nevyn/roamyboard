/*
 * The roamyboard log service: a GATT service that streams the backlog and then every new
 * record to one host, which starts and steers that log session with commands. Every half has
 * it; the right half's host is the left half (relay.c). docs/logging.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>

#include "command.h"
#include "internal.h"
#include "ring.h"

LOG_MODULE_DECLARE(roamy_log, CONFIG_ZMK_LOG_LEVEL);

/** Notifications in flight at once; the rest of the ATT buffers stay free for key reports. */
#define IN_FLIGHT_MAX 2
/** Wait before trying again when the stack has no buffer. */
#define RETRY_MS 50
/** relay ring entries: sequence number (uint32 little-endian), frame flags, then the body. */
#define RELAY_HEADER_LEN 5

K_THREAD_STACK_DEFINE(log_q_stack, CONFIG_ROAMYBOARD_LOG_THREAD_STACK_SIZE);
static struct k_work_q log_q;

void roamy_log_submit(struct k_work *work) { k_work_submit_to_queue(&log_q, work); }

/* lock guards the rings and the session; the log thread, the Bluetooth threads and the log
 * service's work queue all take it. */
static struct k_spinlock lock;
static uint8_t backlog_buf[CONFIG_ROAMYBOARD_LOG_BACKLOG_SIZE];
static struct log_ring backlog = {.buf = backlog_buf, .cap = sizeof(backlog_buf)};
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
static uint8_t relay_buf[CONFIG_ROAMYBOARD_LOG_RELAY_SIZE];
static struct log_ring relay = {.buf = relay_buf, .cap = sizeof(relay_buf)};
#endif

static struct {
    /** The session's host, referenced; NULL while no session is on. */
    struct bt_conn *conn;
    /** Counts sessions, so that a notification of an earlier session cannot count in this one. */
    uint32_t generation;
    struct log_ring_cursor cursor;
    /** Records before this index were in the backlog when the session began. */
    uint32_t backlog_end;
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
    struct log_ring_cursor relay_cursor;
#endif
} session;

static atomic_t in_flight;
static atomic_t in_flight_generation;

static void send_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(send_work, send_handler);

static void wake_sender(void) { k_work_reschedule_for_queue(&log_q, &send_work, K_NO_WAIT); }

void roamy_log_record(const uint8_t *body, size_t len) {
    bool on;
    K_SPINLOCK(&lock) {
        log_ring_push(&backlog, body, len, NULL);
        on = session.conn != NULL;
    }
    if (on) {
        wake_sender();
    }
}

void roamy_log_report(uint8_t level, const char *fmt, ...) {
    char text[LOG_FRAME_TEXT_MAX + 1];
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintk(text, sizeof(text), fmt, args);
    va_end(args);
    uint8_t body[LOG_FRAME_BODY_MAX];
    const size_t body_len =
        log_frame_body(body, sizeof(body), k_uptime_get_32(), level, ROAMY_LOG_SOURCE, text,
                       MIN((size_t)MAX(len, 0), sizeof(text) - 1));
    roamy_log_record(body, body_len);
}

void roamy_log_relay_record(enum log_frame_half half, bool backlog_flag, uint32_t seq,
                            const uint8_t *body, size_t len) {
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
    // Only the receive thread gets here; static keeps its stack small.
    static uint8_t entry[RELAY_HEADER_LEN + LOG_FRAME_BODY_MAX];
    if (len > LOG_FRAME_BODY_MAX) {
        return;
    }
    sys_put_le32(seq, entry);
    entry[4] = (uint8_t)(half | (backlog_flag ? LOG_FRAME_FLAG_BACKLOG : 0));
    memcpy(entry + RELAY_HEADER_LEN, body, len);
    bool on;
    K_SPINLOCK(&lock) {
        on = session.conn != NULL;
        if (on) {
            log_ring_push(&relay, entry, RELAY_HEADER_LEN + len, NULL);
        }
    }
    if (on) {
        wake_sender();
    }
#endif
}

/* GATT service */

static void records_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t write_command(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

BT_GATT_SERVICE_DEFINE(roamyboard_log_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ROAMY_LOG_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ROAMY_LOG_RECORDS_UUID),
                                              BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT,
                                              NULL, NULL, NULL),
                       BT_GATT_CCC(records_ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ROAMY_LOG_COMMAND_UUID),
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_command,
                                              NULL));

/** The records characteristic's value: after the service and its declaration. */
#define RECORDS_ATTR (&roamyboard_log_svc.attrs[2])

/* Sender, on the log service's work queue */

static uint8_t record[RELAY_HEADER_LEN + LOG_FRAME_BODY_MAX];
static struct log_frame_writer writer;
/** The session that writer's record belongs to; 0 while there is none. */
static uint32_t writer_generation;

static void sent(struct bt_conn *conn, void *user_data) {
    if ((atomic_val_t)(uintptr_t)user_data == atomic_get(&in_flight_generation)) {
        atomic_dec(&in_flight);
    }
    wake_sender();
}

/** Loads the next record of session generation into writer; false if there is none. */
static bool next_record(uint32_t generation) {
    bool found = false;
    K_SPINLOCK(&lock) {
        if (session.conn == NULL || session.generation != generation) {
            K_SPINLOCK_BREAK;
        }
        uint32_t index;
        size_t len;
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
        // The relay ring is small, so the right half's records go first.
        len = log_ring_read(&relay, &session.relay_cursor, record, sizeof(record), &index, NULL);
        if (len > RELAY_HEADER_LEN) {
            const uint8_t flags = record[4];
            log_frame_writer_init(&writer, (enum log_frame_half)(flags & LOG_FRAME_FLAG_HALF_MASK),
                                  flags & LOG_FRAME_FLAG_BACKLOG, sys_get_le32(record),
                                  record + RELAY_HEADER_LEN, len - RELAY_HEADER_LEN);
            found = true;
            K_SPINLOCK_BREAK;
        }
#endif
        len = log_ring_read(&backlog, &session.cursor, record, sizeof(record), &index, NULL);
        if (len > 0) {
            log_frame_writer_init(&writer, ROAMY_LOG_HALF,
                                  (int32_t)(index - session.backlog_end) < 0, index, record, len);
            found = true;
        }
    }
    writer_generation = found ? generation : 0;
    return found;
}

static void send_handler(struct k_work *work) {
    while (true) {
        struct bt_conn *conn = NULL;
        uint32_t generation = 0;
        K_SPINLOCK(&lock) {
            if (session.conn) {
                conn = bt_conn_ref(session.conn);
                generation = session.generation;
            }
        }
        if (conn == NULL) {
            writer_generation = 0;
            return;
        }
        if (atomic_get(&in_flight) >= IN_FLIGHT_MAX ||
            !bt_gatt_is_subscribed(conn, RECORDS_ATTR, BT_GATT_CCC_NOTIFY) ||
            ((writer_generation != generation || log_frame_writer_done(&writer)) &&
             !next_record(generation))) {
            bt_conn_unref(conn);
            return;
        }

        uint8_t frag[LOG_FRAME_HEADER_LEN + LOG_FRAME_BODY_MAX];
        const struct log_frame_writer before = writer;
        const size_t cap = MIN(sizeof(frag), (size_t)bt_gatt_get_mtu(conn) - 3);
        const size_t len = log_frame_writer_next(&writer, frag, cap);
        struct bt_gatt_notify_params params = {
            .attr = RECORDS_ATTR,
            .data = frag,
            .len = (uint16_t)len,
            .func = sent,
            .user_data = (void *)(uintptr_t)generation,
        };
        atomic_inc(&in_flight);
        const int err = len > 0 ? bt_gatt_notify_cb(conn, &params) : -EMSGSIZE;
        bt_conn_unref(conn);
        if (err) {
            atomic_dec(&in_flight);
            writer = before;
            // Out of buffers: try again soon. Otherwise the link is going away, which ends the
            // session, or the MTU is below the minimum, which no host has.
            if (err == -ENOMEM || err == -ENOBUFS) {
                k_work_reschedule_for_queue(&log_q, &send_work, K_MSEC(RETRY_MS));
            }
            return;
        }
    }
}

/* Sessions, on the log service's work queue */

static void session_end(void) {
    bool was_on = false;
    K_SPINLOCK(&lock) {
        if (session.conn) {
            bt_conn_unref(session.conn);
            session.conn = NULL;
            session.generation++;
            was_on = true;
        }
    }
    if (!was_on) {
        return;
    }
    atomic_set(&in_flight_generation, 0);
    atomic_set(&in_flight, 0);
    roamy_log_levels_reset();
    roamy_log_report(LOG_LEVEL_INF, "log session ended; every level back to inf");
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
    roamy_log_relay_stop();
#endif
}

static void session_begin(struct bt_conn *conn, const struct log_cmd *cmd, const char *text,
                          size_t text_len) {
    uint32_t from, count, generation;
    K_SPINLOCK(&lock) {
        if (session.conn) {
            bt_conn_unref(session.conn);
        }
        session.conn = bt_conn_ref(conn);
        generation = ++session.generation;
        if (cmd->kind == LOG_CMD_LIVE) {
            log_ring_cursor_end(&backlog, &session.cursor);
        } else if (cmd->has_from[ROAMY_LOG_HALF]) {
            log_ring_cursor_seek(&backlog, &session.cursor, cmd->from[ROAMY_LOG_HALF]);
        } else {
            log_ring_cursor_oldest(&backlog, &session.cursor);
        }
        session.backlog_end = backlog.next;
        from = session.cursor.index;
        count = backlog.next - from;
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
        log_ring_clear(&relay);
        log_ring_cursor_oldest(&relay, &session.relay_cursor);
#endif
    }
    atomic_set(&in_flight_generation, (atomic_val_t)generation);
    atomic_set(&in_flight, 0);
    roamy_log_report(LOG_LEVEL_INF, "log session began at record %u, with %u from the backlog", from,
                   count);
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
    roamy_log_relay_start(text, text_len);
#endif
    wake_sender();
}

struct pending_command {
    struct bt_conn *conn;
    struct log_cmd cmd;
    uint8_t len;
    char text[LOG_CMD_MAX + 1];
};

K_MSGQ_DEFINE(command_msgq, sizeof(struct pending_command), 4, 4);

static void command_handler(struct k_work *work) {
    struct pending_command p;
    while (k_msgq_get(&command_msgq, &p, K_NO_WAIT) == 0) {
        switch (p.cmd.kind) {
        case LOG_CMD_START:
        case LOG_CMD_LIVE:
            session_begin(p.conn, &p.cmd, p.text, p.len);
            break;
        case LOG_CMD_STOP:
            session_end();
            break;
        case LOG_CMD_LEVEL:
            roamy_log_levels_set(p.cmd.module, p.cmd.level);
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
            roamy_log_relay_forward(p.text, p.len);
#endif
            break;
        case LOG_CMD_LIST:
            roamy_log_list();
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOG_RELAY)
            roamy_log_relay_forward(p.text, p.len);
#endif
            break;
        }
        bt_conn_unref(p.conn);
    }
}

static K_WORK_DEFINE(command_work, command_handler);

/** Bluetooth receive thread. */
static ssize_t write_command(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len > LOG_CMD_MAX) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    struct pending_command p = {.len = (uint8_t)len};
    memcpy(p.text, buf, len);
    // A bad command gets an ATT error; a report here would cost the receive thread's stack.
    if (!log_cmd_parse(p.text, len, &p.cmd)) {
        return BT_GATT_ERR(ROAMY_LOG_ATT_ERR_BAD_COMMAND);
    }
    p.conn = bt_conn_ref(conn);
    if (k_msgq_put(&command_msgq, &p, K_NO_WAIT) != 0) {
        bt_conn_unref(p.conn);
        return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }
    roamy_log_submit(&command_work);
    return len;
}

/** Ends the session once its host is no longer subscribed. */
static void subscription_handler(struct k_work *work) {
    struct bt_conn *conn = NULL;
    K_SPINLOCK(&lock) {
        if (session.conn) {
            conn = bt_conn_ref(session.conn);
        }
    }
    if (conn == NULL) {
        return;
    }
    const bool subscribed = bt_gatt_is_subscribed(conn, RECORDS_ATTR, BT_GATT_CCC_NOTIFY);
    bt_conn_unref(conn);
    if (subscribed) {
        wake_sender();
    } else {
        session_end();
    }
}

static K_WORK_DEFINE(subscription_work, subscription_handler);

static void records_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    roamy_log_submit(&subscription_work);
}

static void end_handler(struct k_work *work) { session_end(); }
static K_WORK_DEFINE(end_work, end_handler);

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    bool ours;
    K_SPINLOCK(&lock) { ours = conn == session.conn; }
    if (ours) {
        roamy_log_submit(&end_work);
    }
}

BT_CONN_CB_DEFINE(roamyboard_log) = {
    .disconnected = disconnected,
};

static int log_service_init(void) {
    k_work_queue_start(&log_q, log_q_stack, K_THREAD_STACK_SIZEOF(log_q_stack),
                       K_LOWEST_APPLICATION_THREAD_PRIO, NULL);
    k_thread_name_set(&log_q.thread, "roamy_log");
    return 0;
}

SYS_INIT(log_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
