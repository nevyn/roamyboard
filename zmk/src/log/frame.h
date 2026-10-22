/*
 * Log records and their frames on the roamyboard log service: encoding a record, cutting it
 * into notification-sized fragments, and putting fragments back together. Pure C, without
 * Zephyr, so that the host tests drive it. docs/logging.md states the format as a contract.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** The frame format version in bits 4 to 7 of every fragment's flags. */
#define LOG_FRAME_VERSION 1
/** Flags, fragment index and sequence number. */
#define LOG_FRAME_HEADER_LEN 6
/** The smallest notification payload: the smallest ATT MTU (23) less the notification's 3. */
#define LOG_FRAME_PAYLOAD_MIN 20
/** Longest source name; longer names are cut. */
#define LOG_FRAME_SOURCE_MAX 32
/** Longest text; longer text is cut at a UTF-8 character boundary and marked truncated. */
#define LOG_FRAME_TEXT_MAX 192
/** Uptime, level, source length. */
#define LOG_FRAME_BODY_FIXED 6
#define LOG_FRAME_BODY_MAX (LOG_FRAME_BODY_FIXED + LOG_FRAME_SOURCE_MAX + LOG_FRAME_TEXT_MAX)

/** Bits 0 and 1 of the flags: which half recorded the record. */
enum log_frame_half {
    LOG_FRAME_HALF_UNIBODY = 0,
    LOG_FRAME_HALF_LEFT = 1,
    LOG_FRAME_HALF_RIGHT = 2,
};

#define LOG_FRAME_FLAG_HALF_MASK 0x03
/** The fragment ends its record. */
#define LOG_FRAME_FLAG_LAST 0x04
/** The half recorded the record before the session began. */
#define LOG_FRAME_FLAG_BACKLOG 0x08
#define LOG_FRAME_FLAG_VERSION_SHIFT 4

/** Bit 7 of the body's level byte: the text was cut to LOG_FRAME_TEXT_MAX. */
#define LOG_FRAME_LEVEL_TRUNCATED 0x80
#define LOG_FRAME_LEVEL_MASK 0x07

/**
 * Writes a record body into out: uptime in milliseconds (uint32 little-endian), level (Zephyr's
 * 0 for printk, 1 error to 4 debug), the source name's length, the source name, then the text.
 * The source is cut to LOG_FRAME_SOURCE_MAX bytes and the text to LOG_FRAME_TEXT_MAX, at a
 * UTF-8 character boundary, which sets LOG_FRAME_LEVEL_TRUNCATED. source may be NULL.
 *
 * @param cap  The size of out; LOG_FRAME_BODY_MAX always suffices.
 * @return The body's length, or 0 if cap cannot hold even the fixed part and the source.
 */
size_t log_frame_body(uint8_t *out, size_t cap, uint32_t uptime_ms, uint8_t level,
                      const char *source, const char *text, size_t text_len);

/** A record body, decoded; source and text point into the body and are not NUL-terminated. */
struct log_frame_body_view {
    uint32_t uptime_ms;
    uint8_t level;
    bool truncated;
    const char *source;
    size_t source_len;
    const char *text;
    size_t text_len;
};

/** Decodes a body; false if it is shorter than its fixed part and source name. */
bool log_frame_body_parse(const uint8_t *body, size_t len, struct log_frame_body_view *out);

/** One record on its way out, as fragments. */
struct log_frame_writer {
    uint8_t flags; /* half and backlog; the writer adds the version and LOG_FRAME_FLAG_LAST */
    uint32_t seq;
    const uint8_t *body;
    size_t body_len;
    size_t offset; /* body bytes already written */
    uint8_t index; /* the next fragment's index */
};

/** Starts writing a record of body_len bytes (1 to LOG_FRAME_BODY_MAX). */
void log_frame_writer_init(struct log_frame_writer *w, enum log_frame_half half, bool backlog,
                           uint32_t seq, const uint8_t *body, size_t body_len);

/** Whether every fragment of the record has been written. */
bool log_frame_writer_done(const struct log_frame_writer *w);

/**
 * Writes the next fragment into out, at most cap bytes, and advances the writer: the header
 * (flags, fragment index, the sequence number as uint32 little-endian), then as much of the
 * body as fits.
 *
 * @param cap  The notification payload size: ATT MTU less 3, at least LOG_FRAME_PAYLOAD_MIN.
 * @return The fragment's length, or 0 when the record is done or cap is below
 *         LOG_FRAME_PAYLOAD_MIN.
 */
size_t log_frame_writer_next(struct log_frame_writer *w, uint8_t *out, size_t cap);

/** Outcome of feeding one fragment to a reassembler. */
enum log_frame_result {
    /** The fragment is part of a record that is not complete yet. */
    LOG_FRAME_PARTIAL,
    /** The fragment completed a record: half, backlog, seq, body and len hold it. */
    LOG_FRAME_COMPLETE,
    /** Shorter than a header, or a body that does not parse; dropped. */
    LOG_FRAME_MALFORMED,
    /** A frame format version other than LOG_FRAME_VERSION; dropped. */
    LOG_FRAME_BAD_VERSION,
    /** A continuation that does not follow the record in progress; dropped with it. */
    LOG_FRAME_OUT_OF_ORDER,
    /** The record grew past LOG_FRAME_BODY_MAX; dropped. */
    LOG_FRAME_TOO_LONG,
};

/** Puts fragments back together into records, one record at a time. */
struct log_frame_reassembler {
    bool active;
    uint8_t next_index;
    enum log_frame_half half;
    bool backlog;
    uint32_t seq;
    size_t len;
    uint8_t body[LOG_FRAME_BODY_MAX];
    /** Records whose first fragments arrived but which another record's first fragment cut
     * short; they are lost. */
    uint32_t incomplete;
};

void log_frame_reassembler_init(struct log_frame_reassembler *r);

/** Feeds one fragment. After LOG_FRAME_COMPLETE, the record stays in r until the next feed. */
enum log_frame_result log_frame_reassemble(struct log_frame_reassembler *r, const uint8_t *frag,
                                           size_t len);
