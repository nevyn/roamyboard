/*
 * SPDX-License-Identifier: MIT
 */

#include "ring.h"

#include <string.h>

/** Index comparisons that survive the uint32 wrap. */
static bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

static void copy_out(const struct log_ring *r, size_t offset, uint8_t *out, size_t len) {
    const size_t first = len < r->cap - offset ? len : r->cap - offset;
    memcpy(out, r->buf + offset, first);
    memcpy(out + first, r->buf, len - first);
}

static void copy_in(struct log_ring *r, size_t offset, const uint8_t *in, size_t len) {
    const size_t first = len < r->cap - offset ? len : r->cap - offset;
    memcpy(r->buf + offset, in, first);
    memcpy(r->buf, in + first, len - first);
}

static size_t wrap(const struct log_ring *r, size_t offset) {
    return offset >= r->cap ? offset - r->cap : offset;
}

static size_t entry_len(const struct log_ring *r, size_t offset) {
    uint8_t prefix[LOG_RING_ENTRY_OVERHEAD];
    copy_out(r, offset, prefix, sizeof(prefix));
    return (size_t)prefix[0] | (size_t)prefix[1] << 8;
}

void log_ring_init(struct log_ring *r, uint8_t *buf, size_t cap) {
    *r = (struct log_ring){.buf = buf, .cap = cap};
}

void log_ring_clear(struct log_ring *r) {
    r->tail = 0;
    r->used = 0;
    r->oldest = r->next;
}

static void drop_oldest(struct log_ring *r) {
    const size_t total = LOG_RING_ENTRY_OVERHEAD + entry_len(r, r->tail);
    r->tail = wrap(r, r->tail + total);
    r->used -= total;
    r->oldest++;
}

bool log_ring_push(struct log_ring *r, const uint8_t *data, size_t len, uint32_t *index) {
    const size_t total = LOG_RING_ENTRY_OVERHEAD + len;
    if (len == 0 || len > 0xFFFF || total > r->cap) {
        return false;
    }
    while (r->cap - r->used < total) {
        drop_oldest(r);
    }
    const size_t head = wrap(r, r->tail + r->used);
    const uint8_t prefix[LOG_RING_ENTRY_OVERHEAD] = {len & 0xFF, len >> 8 & 0xFF};
    copy_in(r, head, prefix, sizeof(prefix));
    copy_in(r, wrap(r, head + LOG_RING_ENTRY_OVERHEAD), data, len);
    r->used += total;
    if (index) {
        *index = r->next;
    }
    r->next++;
    return true;
}

uint32_t log_ring_count(const struct log_ring *r) { return r->next - r->oldest; }

void log_ring_cursor_oldest(const struct log_ring *r, struct log_ring_cursor *c) {
    c->index = r->oldest;
    c->offset = r->tail;
}

void log_ring_cursor_end(const struct log_ring *r, struct log_ring_cursor *c) {
    c->index = r->next;
    c->offset = wrap(r, r->tail + r->used);
}

void log_ring_cursor_seek(const struct log_ring *r, struct log_ring_cursor *c, uint32_t index) {
    log_ring_cursor_oldest(r, c);
    if (before(index, r->oldest) || before(r->next, index)) {
        return;
    }
    while (c->index != index) {
        c->offset = wrap(r, c->offset + LOG_RING_ENTRY_OVERHEAD + entry_len(r, c->offset));
        c->index++;
    }
}

size_t log_ring_read(const struct log_ring *r, struct log_ring_cursor *c, uint8_t *out, size_t cap,
                     uint32_t *index, uint32_t *skipped) {
    uint32_t missed = 0;
    if (before(c->index, r->oldest)) {
        missed = r->oldest - c->index;
        log_ring_cursor_oldest(r, c);
    }
    if (skipped) {
        *skipped = missed;
    }
    if (!before(c->index, r->next)) {
        return 0;
    }
    const size_t len = entry_len(r, c->offset);
    copy_out(r, wrap(r, c->offset + LOG_RING_ENTRY_OVERHEAD), out, len < cap ? len : cap);
    *index = c->index;
    c->offset = wrap(r, c->offset + LOG_RING_ENTRY_OVERHEAD + len);
    c->index++;
    return len < cap ? len : cap;
}
