/*
 * A byte ring of variable-length entries that drops its oldest entries to make room. Every
 * entry gets the next index, from 0 upwards; a half's own log records use it as their sequence
 * number. Pure C, without Zephyr and without locking: the caller serializes access.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Each entry's length prefix: uint16 little-endian. */
#define LOG_RING_ENTRY_OVERHEAD 2

struct log_ring {
    uint8_t *buf;
    size_t cap;
    size_t tail; /* offset of the oldest entry */
    size_t used; /* bytes in use, prefixes included */
    uint32_t oldest; /* index of the oldest entry; equals next when empty */
    uint32_t next; /* index that the next push gets */
};

/** A reader's position: the index of the next entry to read, and its offset while it exists. */
struct log_ring_cursor {
    uint32_t index;
    size_t offset;
};

void log_ring_init(struct log_ring *r, uint8_t *buf, size_t cap);

/** Drops every entry; indexes go on from where they were. */
void log_ring_clear(struct log_ring *r);

/**
 * Appends an entry of len bytes, dropping the oldest entries until it fits.
 *
 * @param index  Receives the entry's index; may be NULL.
 * @return false, and nothing changes, if len is 0, over 65535, or the entry cannot fit even in
 *         the empty ring.
 */
bool log_ring_push(struct log_ring *r, const uint8_t *data, size_t len, uint32_t *index);

/** How many entries the ring holds. */
uint32_t log_ring_count(const struct log_ring *r);

/** Places c at the oldest entry. */
void log_ring_cursor_oldest(const struct log_ring *r, struct log_ring_cursor *c);

/** Places c after the newest entry, so that it reads only entries pushed from now on. */
void log_ring_cursor_end(const struct log_ring *r, struct log_ring_cursor *c);

/**
 * Places c at the entry with index, if the ring holds it or index is the next index. An index
 * older than the oldest entry places c at the oldest entry; an index beyond the next one (such
 * as one from before a restart) places it at the oldest entry too.
 */
void log_ring_cursor_seek(const struct log_ring *r, struct log_ring_cursor *c, uint32_t index);

/**
 * Copies the entry at c into out and moves c past it. If the ring has dropped entries that c had
 * not read yet, c first moves to the oldest entry.
 *
 * @param index    Receives the entry's index.
 * @param skipped  Receives how many entries c missed because the ring dropped them; may be NULL.
 * @return The entry's length, or 0 if c is at the end. An entry longer than cap is cut to cap.
 */
size_t log_ring_read(const struct log_ring *r, struct log_ring_cursor *c, uint8_t *out, size_t cap,
                     uint32_t *index, uint32_t *skipped);
