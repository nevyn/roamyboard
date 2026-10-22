/*
 * Commands that a host writes to the log service's command characteristic, one ASCII command
 * per write. Pure C, without Zephyr. docs/logging.md lists them.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "frame.h"

/** The longest command. */
#define LOG_CMD_MAX 64

enum log_cmd_kind {
    /** "start", or "start L:12 R:40": begin a session from each named half's record, or from
     * the oldest record in the backlog. */
    LOG_CMD_START,
    /** "live": begin a session without the backlog. */
    LOG_CMD_LIVE,
    /** "stop": end the session; levels return to info. */
    LOG_CMD_STOP,
    /** "MODULE=LEVEL" or "all=LEVEL". */
    LOG_CMD_LEVEL,
    /** "list": log every source and its level. */
    LOG_CMD_LIST,
};

struct log_cmd {
    enum log_cmd_kind kind;
    /** LOG_CMD_START: whether a half (enum log_frame_half) has a starting record, and which. */
    bool has_from[3];
    uint32_t from[3];
    /** LOG_CMD_LEVEL: the source name, or "all". */
    char module[LOG_FRAME_SOURCE_MAX + 1];
    /** LOG_CMD_LEVEL: 0 off, 1 err, 2 wrn, 3 inf, 4 dbg. */
    uint8_t level;
};

/** Parses one command; false if text is not one of the commands above. */
bool log_cmd_parse(const char *text, size_t len, struct log_cmd *out);

/** "off", "err", "wrn", "inf", "dbg" for 0 to 4, "raw" for anything else. */
const char *log_cmd_level_name(uint8_t level);
