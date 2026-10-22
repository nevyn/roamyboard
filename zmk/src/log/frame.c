/*
 * SPDX-License-Identifier: MIT
 */

#include "frame.h"

#include <string.h>

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = value & 0xFF;
    out[1] = value >> 8 & 0xFF;
    out[2] = value >> 16 & 0xFF;
    out[3] = value >> 24 & 0xFF;
}

static uint32_t get_u32(const uint8_t *in) {
    return (uint32_t)in[0] | (uint32_t)in[1] << 8 | (uint32_t)in[2] << 16 | (uint32_t)in[3] << 24;
}

/** The longest prefix of text, at most max bytes, that does not end inside a character. */
static size_t utf8_cut(const char *text, size_t len, size_t max) {
    if (len <= max) {
        return len;
    }
    size_t cut = max;
    while (cut > 0 && ((uint8_t)text[cut] & 0xC0) == 0x80) {
        cut--;
    }
    return cut;
}

size_t log_frame_body(uint8_t *out, size_t cap, uint32_t uptime_ms, uint8_t level,
                      const char *source, const char *text, size_t text_len) {
    const size_t source_len = source ? utf8_cut(source, strlen(source), LOG_FRAME_SOURCE_MAX) : 0;
    if (cap < LOG_FRAME_BODY_FIXED + source_len) {
        return 0;
    }
    size_t room = cap - LOG_FRAME_BODY_FIXED - source_len;
    if (room > LOG_FRAME_TEXT_MAX) {
        room = LOG_FRAME_TEXT_MAX;
    }
    const size_t kept = utf8_cut(text, text_len, room);
    put_u32(out, uptime_ms);
    out[4] = (level & LOG_FRAME_LEVEL_MASK) | (kept < text_len ? LOG_FRAME_LEVEL_TRUNCATED : 0);
    out[5] = (uint8_t)source_len;
    if (source_len > 0) {
        memcpy(out + LOG_FRAME_BODY_FIXED, source, source_len);
    }
    if (kept > 0) {
        memcpy(out + LOG_FRAME_BODY_FIXED + source_len, text, kept);
    }
    return LOG_FRAME_BODY_FIXED + source_len + kept;
}

bool log_frame_body_parse(const uint8_t *body, size_t len, struct log_frame_body_view *out) {
    if (len < LOG_FRAME_BODY_FIXED || len < LOG_FRAME_BODY_FIXED + (size_t)body[5]) {
        return false;
    }
    out->uptime_ms = get_u32(body);
    out->level = body[4] & LOG_FRAME_LEVEL_MASK;
    out->truncated = (body[4] & LOG_FRAME_LEVEL_TRUNCATED) != 0;
    out->source_len = body[5];
    out->source = (const char *)body + LOG_FRAME_BODY_FIXED;
    out->text = out->source + out->source_len;
    out->text_len = len - LOG_FRAME_BODY_FIXED - out->source_len;
    return true;
}

void log_frame_writer_init(struct log_frame_writer *w, enum log_frame_half half, bool backlog,
                           uint32_t seq, const uint8_t *body, size_t body_len) {
    *w = (struct log_frame_writer){
        .flags = (uint8_t)((half & LOG_FRAME_FLAG_HALF_MASK) | (backlog ? LOG_FRAME_FLAG_BACKLOG : 0)),
        .seq = seq,
        .body = body,
        .body_len = body_len,
    };
}

bool log_frame_writer_done(const struct log_frame_writer *w) {
    return w->index > 0 && w->offset >= w->body_len;
}

size_t log_frame_writer_next(struct log_frame_writer *w, uint8_t *out, size_t cap) {
    if (log_frame_writer_done(w) || cap < LOG_FRAME_PAYLOAD_MIN) {
        return 0;
    }
    size_t chunk = w->body_len - w->offset;
    if (chunk > cap - LOG_FRAME_HEADER_LEN) {
        chunk = cap - LOG_FRAME_HEADER_LEN;
    }
    const bool last = w->offset + chunk >= w->body_len;
    out[0] = (uint8_t)(w->flags | (last ? LOG_FRAME_FLAG_LAST : 0) |
                       LOG_FRAME_VERSION << LOG_FRAME_FLAG_VERSION_SHIFT);
    out[1] = w->index;
    put_u32(out + 2, w->seq);
    memcpy(out + LOG_FRAME_HEADER_LEN, w->body + w->offset, chunk);
    w->offset += chunk;
    w->index++;
    return LOG_FRAME_HEADER_LEN + chunk;
}

void log_frame_reassembler_init(struct log_frame_reassembler *r) {
    memset(r, 0, sizeof(*r));
}

enum log_frame_result log_frame_reassemble(struct log_frame_reassembler *r, const uint8_t *frag,
                                           size_t len) {
    if (len < LOG_FRAME_HEADER_LEN) {
        r->active = false;
        return LOG_FRAME_MALFORMED;
    }
    if (frag[0] >> LOG_FRAME_FLAG_VERSION_SHIFT != LOG_FRAME_VERSION) {
        r->active = false;
        return LOG_FRAME_BAD_VERSION;
    }
    const uint8_t index = frag[1];
    const uint32_t seq = get_u32(frag + 2);
    const enum log_frame_half half = (enum log_frame_half)(frag[0] & LOG_FRAME_FLAG_HALF_MASK);
    if (index == 0) {
        if (r->active) {
            r->incomplete++;
        }
        r->active = true;
        r->half = half;
        r->backlog = (frag[0] & LOG_FRAME_FLAG_BACKLOG) != 0;
        r->seq = seq;
        r->len = 0;
        r->next_index = 0;
    } else if (!r->active || index != r->next_index || seq != r->seq || half != r->half) {
        r->active = false;
        return LOG_FRAME_OUT_OF_ORDER;
    }
    const size_t chunk = len - LOG_FRAME_HEADER_LEN;
    if (r->len + chunk > sizeof(r->body)) {
        r->active = false;
        return LOG_FRAME_TOO_LONG;
    }
    memcpy(r->body + r->len, frag + LOG_FRAME_HEADER_LEN, chunk);
    r->len += chunk;
    r->next_index++;
    if (!(frag[0] & LOG_FRAME_FLAG_LAST)) {
        return LOG_FRAME_PARTIAL;
    }
    r->active = false;
    struct log_frame_body_view view;
    return log_frame_body_parse(r->body, r->len, &view) ? LOG_FRAME_COMPLETE : LOG_FRAME_MALFORMED;
}
