/*
 * The roamyboard log backend: turns every log message that passes its runtime filter into a
 * record in the backlog (service.c). Its filter starts at info for every source; commands from
 * the host move it per source. docs/logging.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_internal.h>
#include <zephyr/logging/log_msg.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/sys/cbprintf.h>

#include "command.h"
#include "internal.h"

LOG_MODULE_REGISTER(roamy_log, CONFIG_ZMK_LOG_LEVEL);

static void process(const struct log_backend *const backend, union log_msg_generic *msg);
static void dropped(const struct log_backend *const backend, uint32_t cnt);
static void panic(const struct log_backend *const backend) {}

static const struct log_backend_api api = {
    .process = process,
    .dropped = dropped,
    .panic = panic,
};

LOG_BACKEND_DEFINE(roamyboard_log_backend, api, true);

/** Sets this backend's filter for source to level; returns the level that it got, which the
 * source's compiled level caps. */
static uint8_t filter_set(uint16_t source, uint8_t level) {
    return (uint8_t)log_filter_set(&roamyboard_log_backend, Z_LOG_LOCAL_DOMAIN_ID, source, level);
}

void roamy_log_levels_reset(void) {
    for (uint16_t s = 0; s < log_src_cnt_get(Z_LOG_LOCAL_DOMAIN_ID); s++) {
        filter_set(s, LOG_LEVEL_INF);
    }
}

void roamy_log_levels_set(const char *module, uint8_t level) {
    const uint16_t count = (uint16_t)log_src_cnt_get(Z_LOG_LOCAL_DOMAIN_ID);
    if (strcmp(module, "all") == 0) {
        uint16_t capped = 0;
        for (uint16_t s = 0; s < count; s++) {
            capped += filter_set(s, level) < level;
        }
        roamy_log_report(LOG_LEVEL_INF, "level of all %u sources: %s (%u compiled with less)", count,
                       log_cmd_level_name(level), capped);
        return;
    }
    const int source = log_source_id_get(module);
    if (source < 0) {
        roamy_log_report(LOG_LEVEL_WRN, "no log source %s on this half; roamy-log --list-modules "
                                      "lists them",
                       module);
        return;
    }
    const uint8_t got = filter_set((uint16_t)source, level);
    if (got < level) {
        roamy_log_report(LOG_LEVEL_INF, "level of %s: %s, the most that it was compiled with",
                       module, log_cmd_level_name(got));
    } else {
        roamy_log_report(LOG_LEVEL_INF, "level of %s: %s", module, log_cmd_level_name(got));
    }
}

void roamy_log_list(void) {
    const uint16_t count = (uint16_t)log_src_cnt_get(Z_LOG_LOCAL_DOMAIN_ID);
    char line[LOG_FRAME_TEXT_MAX - 24];
    size_t len = 0;
    uint16_t first = 0;
    for (uint16_t s = 0; s <= count; s++) {
        const char *name = s < count ? log_source_name_get(Z_LOG_LOCAL_DOMAIN_ID, s) : NULL;
        const char *level = s < count ? log_cmd_level_name((uint8_t)log_filter_get(
                                            &roamyboard_log_backend, Z_LOG_LOCAL_DOMAIN_ID, s, true))
                                      : NULL;
        const size_t need = name ? strlen(name) + 1 + strlen(level) + 1 : 0;
        if (len > 0 && (name == NULL || len + need >= sizeof(line))) {
            line[len - 1] = '\0';
            roamy_log_report(LOG_LEVEL_INF, "sources %u-%u of %u: %s", first + 1, s, count, line);
            len = 0;
            first = s;
        }
        if (name && need < sizeof(line)) {
            len += (size_t)snprintf(line + len, sizeof(line) - len, "%s=%s ", name, level);
        }
    }
}

/* The log processing thread only, from here on. */

struct text {
    /* One byte over the frame's limit, so that log_frame_body() sees the cut. */
    char data[LOG_FRAME_TEXT_MAX + 1];
    size_t len;
};

static int text_out(int c, void *ctx) {
    struct text *text = ctx;
    if (text->len < sizeof(text->data)) {
        text->data[text->len++] = (char)c;
    }
    return 0;
}

static void text_hex(struct text *text, const uint8_t *data, size_t len) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len && text->len + 3 <= sizeof(text->data); i++) {
        text->data[text->len++] = ' ';
        text->data[text->len++] = digits[data[i] >> 4];
        text->data[text->len++] = digits[data[i] & 0xF];
    }
}

/** The message's uptime in milliseconds, from its age in the log's own timestamp, which wraps. */
static uint32_t uptime_of(struct log_msg *msg) {
    const log_timestamp_t age = z_log_timestamp() - log_msg_get_timestamp(msg);
    return (uint32_t)(k_uptime_get() - (int64_t)(log_output_timestamp_to_us(age) / 1000));
}

static bool filters_ready;

static void process(const struct log_backend *const backend, union log_msg_generic *generic) {
    // The core enables every backend at CONFIG_LOG_MAX_LEVEL; this one starts at info instead.
    if (!filters_ready) {
        filters_ready = true;
        roamy_log_levels_reset();
    }
    struct log_msg *msg = &generic->log;
    const uint8_t level = log_msg_get_level(msg);
    const uint8_t domain = log_msg_get_domain(msg);
    const int16_t source = log_msg_get_source_id(msg);
    if (source >= 0 && level > log_filter_get(backend, domain, source, true)) {
        return;
    }

    static struct text text;
    text.len = 0;
    size_t len;
    uint8_t *package = log_msg_get_package(msg, &len);
    if (len > 0) {
        cbpprintf(text_out, &text, package);
    }
    uint8_t *data = log_msg_get_data(msg, &len);
    if (len > 0) {
        text_hex(&text, data, len);
    }
    while (text.len > 0 && (text.data[text.len - 1] == '\n' || text.data[text.len - 1] == '\r')) {
        text.len--;
    }

    static uint8_t body[LOG_FRAME_BODY_MAX];
    const char *name = source >= 0 ? log_source_name_get(domain, source) : NULL;
    const size_t body_len =
        log_frame_body(body, sizeof(body), uptime_of(msg), level, name, text.data, text.len);
    roamy_log_record(body, body_len);
}

static void dropped(const struct log_backend *const backend, uint32_t cnt) {
    char text[64];
    const int len = snprintk(text, sizeof(text), "%u log messages dropped before this backend",
                             cnt);
    uint8_t body[LOG_FRAME_BODY_MAX];
    const size_t body_len = log_frame_body(body, sizeof(body), k_uptime_get_32(), LOG_LEVEL_WRN,
                                           ROAMY_LOG_SOURCE, text, (size_t)len);
    roamy_log_record(body, body_len);
}
