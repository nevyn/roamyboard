/*
 * Shared between the log backend (backend.c), the log service (service.c) and the relay of the
 * right half's records (relay.c). docs/logging.md describes them.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>

#include "frame.h"

#define ROAMY_LOG_UUID(num) BT_UUID_128_ENCODE(num, 0x68d2, 0x4d86, 0xa34c, 0x647827c1b55b)
#define ROAMY_LOG_SERVICE_UUID ROAMY_LOG_UUID(0x7beba2b0)
/** Notify: the records, as frames (frame.h). */
#define ROAMY_LOG_RECORDS_UUID ROAMY_LOG_UUID(0x7beba2b1)
/** Write, with or without response: one command (command.h) per write. */
#define ROAMY_LOG_COMMAND_UUID ROAMY_LOG_UUID(0x7beba2b2)

/** The ATT error that a write of a command that does not parse gets. */
#define ROAMY_LOG_ATT_ERR_BAD_COMMAND 0x80

/** The source name of the records that the log service writes about itself. */
#define ROAMY_LOG_SOURCE "roamy_log"

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define ROAMY_LOG_HALF LOG_FRAME_HALF_LEFT
#elif IS_ENABLED(CONFIG_ZMK_SPLIT)
#define ROAMY_LOG_HALF LOG_FRAME_HALF_RIGHT
#else
#define ROAMY_LOG_HALF LOG_FRAME_HALF_UNIBODY
#endif

/* service.c */

/** Appends a record body (log_frame_body()) to the backlog and wakes the sender. Any thread. */
void roamy_log_record(const uint8_t *body, size_t len);

/**
 * Writes a record about the log service itself, at level (Zephyr's 1 to 4), from source
 * ROAMY_LOG_SOURCE, straight into the backlog: it bypasses the log filters and the other
 * backends. Any thread; not from the log backend's process().
 */
void roamy_log_report(uint8_t level, const char *fmt, ...);

/**
 * Queues one of the right half's records for the session's host; dropped when no session is
 * on. The Bluetooth receive thread.
 */
void roamy_log_relay_record(enum log_frame_half half, bool backlog, uint32_t seq,
                            const uint8_t *body, size_t len);

/** Submits work to the log service's own work queue. */
void roamy_log_submit(struct k_work *work);

/* backend.c; the log service's work queue only */

/** Sets this half's level for the source named module, or for every source if it is "all". */
void roamy_log_levels_set(const char *module, uint8_t level);

/** Sets every source back to info, without a report. */
void roamy_log_levels_reset(void);

/** Reports every source of this half with its level. */
void roamy_log_list(void);

/* relay.c, on the split central; the log service's work queue only */

/** Starts the right half's session with the host's start command, now or once it connects. */
void roamy_log_relay_start(const char *cmd, size_t len);

/** Ends the right half's session. */
void roamy_log_relay_stop(void);

/** Forwards a level or list command to the right half, if it is connected. */
void roamy_log_relay_forward(const char *cmd, size_t len);
