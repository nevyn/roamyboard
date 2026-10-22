/*
 * SPDX-License-Identifier: MIT
 */

#include "command.h"

#include <string.h>

static const char *const level_names[] = {"off", "err", "wrn", "inf", "dbg"};

const char *log_cmd_level_name(uint8_t level) {
    return level < sizeof(level_names) / sizeof(level_names[0]) ? level_names[level] : "raw";
}

static bool equals(const char *text, size_t len, const char *word) {
    return strlen(word) == len && memcmp(text, word, len) == 0;
}

/** Parses "U:n", "L:n" or "R:n" into out. */
static bool parse_from(const char *text, size_t len, struct log_cmd *out) {
    static const char halves[] = {'U', 'L', 'R'};
    if (len < 3 || text[1] != ':' || len > 2 + 10) {
        return false;
    }
    int half = -1;
    for (int i = 0; i < 3; i++) {
        if (text[0] == halves[i]) {
            half = i;
        }
    }
    if (half < 0) {
        return false;
    }
    uint64_t value = 0;
    for (size_t i = 2; i < len; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = value * 10 + (uint64_t)(text[i] - '0');
    }
    if (value > UINT32_MAX) {
        return false;
    }
    out->has_from[half] = true;
    out->from[half] = (uint32_t)value;
    return true;
}

static bool is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '-';
}

bool log_cmd_parse(const char *text, size_t len, struct log_cmd *out) {
    memset(out, 0, sizeof(*out));
    if (len == 0 || len > LOG_CMD_MAX) {
        return false;
    }
    if (equals(text, len, "live")) {
        out->kind = LOG_CMD_LIVE;
        return true;
    }
    if (equals(text, len, "stop")) {
        out->kind = LOG_CMD_STOP;
        return true;
    }
    if (equals(text, len, "list")) {
        out->kind = LOG_CMD_LIST;
        return true;
    }
    if (len >= 5 && memcmp(text, "start", 5) == 0 && (len == 5 || text[5] == ' ')) {
        out->kind = LOG_CMD_START;
        size_t pos = 5;
        while (pos < len) {
            while (pos < len && text[pos] == ' ') {
                pos++;
            }
            size_t end = pos;
            while (end < len && text[end] != ' ') {
                end++;
            }
            if (end > pos && !parse_from(text + pos, end - pos, out)) {
                return false;
            }
            pos = end;
        }
        return true;
    }
    const char *eq = memchr(text, '=', len);
    if (eq == NULL || eq == text || (size_t)(eq - text) > LOG_FRAME_SOURCE_MAX) {
        return false;
    }
    const size_t name_len = (size_t)(eq - text);
    for (size_t i = 0; i < name_len; i++) {
        if (!is_name_char(text[i])) {
            return false;
        }
    }
    const char *level = eq + 1;
    const size_t level_len = len - name_len - 1;
    for (uint8_t i = 0; i < sizeof(level_names) / sizeof(level_names[0]); i++) {
        if (equals(level, level_len, level_names[i])) {
            out->kind = LOG_CMD_LEVEL;
            memcpy(out->module, text, name_len);
            out->level = i;
            return true;
        }
    }
    return false;
}
