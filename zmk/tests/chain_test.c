/*
 * Host tests for zmk/drivers/kscan/chain.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "chain.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        long long a_ = (long long)(a), b_ = (long long)(b);                                        \
        if (a_ != b_) {                                                                            \
            fprintf(stderr, "%s:%d: CHECK_EQ failed in %s: %s == %lld, expected %s == %lld\n",     \
                    __FILE__, __LINE__, __func__, #a, a_, #b, b_);                                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define MAX_ROWS 7
#define MAX_COLUMNS 30
#define MAX_KEYS (MAX_ROWS * MAX_COLUMNS)
#define MAX_MODULES 32

/* A 5-key module byte with the given switches closed; sw is 1-based (SW1 = bit 0). */
#define SW(n) (1u << ((n) - 1))

struct event {
    int row, column;
    bool pressed;
};

struct recorder {
    struct event events[MAX_KEYS * 2];
    int count;
};

static void record(void *ctx, int row, int column, bool pressed) {
    struct recorder *rec = ctx;
    rec->events[rec->count++] = (struct event){row, column, pressed};
}

struct fixture {
    struct chain chain;
    bool reported[MAX_KEYS];
    bool active[MAX_KEYS];
    struct recorder rec;
};

static void setup(struct fixture *f, enum chain_anchor anchor, int columns, int stable_scans) {
    memset(f, 0, sizeof(*f));
    const struct chain_config config = {
        .anchor = anchor,
        .columns = columns,
        .rows = 5,
        .stable_scans = stable_scans,
    };
    chain_init(&f->chain, &config, f->reported);
}

/* Builds a chain read: the given module bytes, then 0xFF to the end as the terminator gives. */
static size_t read_of(uint8_t *buf, const uint8_t *modules, int count) {
    memset(buf, 0xFF, MAX_MODULES + 1);
    memcpy(buf, modules, (size_t)count);
    return MAX_MODULES + 1;
}

static enum chain_scan_result scan(struct fixture *f, const uint8_t *modules, int count) {
    uint8_t buf[MAX_MODULES + 1];
    const size_t len = read_of(buf, modules, count);
    return chain_scan(&f->chain, buf, len, f->active);
}

/* Scans until the count is accepted, then once more; returns the result of the last scan. */
static enum chain_scan_result settle(struct fixture *f, const uint8_t *modules, int count) {
    for (int i = 0; i < f->chain.config.stable_scans; i++) {
        scan(f, modules, count);
    }
    return scan(f, modules, count);
}

static bool active_at(const struct fixture *f, int row, int column) {
    return f->active[row * f->chain.config.columns + column];
}

static int active_total(const struct fixture *f) {
    int n = 0;
    for (int i = 0; i < f->chain.config.rows * f->chain.config.columns; i++) {
        n += f->active[i];
    }
    return n;
}

static void test_sentinel_search(void) {
    const uint8_t none[] = {0x00, 0x1F, 0x7F};
    CHECK_EQ(chain_count_key_modules(none, sizeof(none)), CHAIN_COUNT_FAULT);

    const uint8_t zero[] = {0xFF, 0xFF};
    CHECK_EQ(chain_count_key_modules(zero, sizeof(zero)), 0);

    const uint8_t two[] = {0x00, 0x1F, 0xFF, 0x00};
    CHECK_EQ(chain_count_key_modules(two, sizeof(two)), 2);

    /* Any byte with bit 7 set ends the chain, not only 0xFF. */
    const uint8_t partial[] = {0x01, 0x80};
    CHECK_EQ(chain_count_key_modules(partial, sizeof(partial)), 1);

    CHECK_EQ(chain_count_key_modules(zero, 0), CHAIN_COUNT_FAULT);
}

static void test_zero_modules(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 30, 3);
    CHECK_EQ(scan(&f, NULL, 0), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, NULL, 0), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, NULL, 0), CHAIN_SCAN_REMAP);
    CHECK_EQ(f.chain.accepted, 0);
    CHECK_EQ(scan(&f, NULL, 0), CHAIN_SCAN_KEYS);
    CHECK_EQ(active_total(&f), 0);
}

static void test_one_module_rows(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 30, 3);
    const uint8_t modules[] = {SW(5) | SW(1)};
    CHECK_EQ(settle(&f, modules, 1), CHAIN_SCAN_KEYS);
    CHECK_EQ(f.chain.accepted, 1);
    CHECK(active_at(&f, 0, 29)); /* SW5, the top key, is row 0 */
    CHECK(active_at(&f, 4, 29)); /* SW1, the bottom key, is row 4 */
    CHECK_EQ(active_total(&f), 2);

    const uint8_t sw3[] = {SW(3)};
    CHECK_EQ(scan(&f, sw3, 1), CHAIN_SCAN_KEYS);
    CHECK(active_at(&f, 2, 29));
    CHECK_EQ(active_total(&f), 1);
}

static void test_n_modules_anchor_mcu(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 30, 3);
    const uint8_t modules[] = {SW(1), 0x00, SW(2), SW(4)};
    CHECK_EQ(settle(&f, modules, 4), CHAIN_SCAN_KEYS);
    CHECK(active_at(&f, 4, 29)); /* physical 0, nearest the MCU module: rightmost column */
    CHECK(active_at(&f, 3, 27)); /* physical 2 */
    CHECK(active_at(&f, 1, 26)); /* physical 3 */
    CHECK_EQ(active_total(&f), 3);
}

static void test_n_modules_anchor_terminator(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_TERMINATOR, 15, 3);
    const uint8_t modules[] = {SW(1), 0x00, SW(2), SW(4)};
    CHECK_EQ(settle(&f, modules, 4), CHAIN_SCAN_KEYS);
    CHECK(active_at(&f, 4, 3)); /* physical 0, nearest the MCU module: column count - 1 */
    CHECK(active_at(&f, 3, 1)); /* physical 2 */
    CHECK(active_at(&f, 1, 0)); /* physical 3, nearest the terminator: column 0 */
    CHECK_EQ(active_total(&f), 3);
}

static void test_column_mapping(void) {
    const struct chain_config mcu = {CHAIN_ANCHOR_MCU, 15, 5, 3};
    CHECK_EQ(chain_keymap_column(&mcu, 3, 0), 14);
    CHECK_EQ(chain_keymap_column(&mcu, 3, 2), 12);
    CHECK_EQ(chain_keymap_column(&mcu, 20, 14), 0);
    CHECK_EQ(chain_keymap_column(&mcu, 20, 15), -1);

    const struct chain_config term = {CHAIN_ANCHOR_TERMINATOR, 15, 5, 3};
    CHECK_EQ(chain_keymap_column(&term, 3, 0), 2);
    CHECK_EQ(chain_keymap_column(&term, 3, 2), 0);
    CHECK_EQ(chain_keymap_column(&term, 20, 19), 0);
    CHECK_EQ(chain_keymap_column(&term, 20, 5), 14);
    CHECK_EQ(chain_keymap_column(&term, 20, 4), -1);
}

static void test_more_modules_than_columns(void) {
    uint8_t modules[20];
    memset(modules, SW(1), sizeof(modules));

    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 15, 3);
    CHECK_EQ(settle(&f, modules, 20), CHAIN_SCAN_KEYS);
    CHECK_EQ(f.chain.accepted, 20);
    CHECK_EQ(active_total(&f), 15); /* physical 15..19 are ignored */

    setup(&f, CHAIN_ANCHOR_TERMINATOR, 15, 3);
    modules[0] = SW(5); /* physical 0 maps past the last column */
    CHECK_EQ(settle(&f, modules, 20), CHAIN_SCAN_KEYS);
    CHECK_EQ(active_total(&f), 15);
    for (int column = 0; column < 15; column++) {
        CHECK(active_at(&f, 4, column));
        CHECK(!active_at(&f, 0, column));
    }
}

static void test_missing_sentinel(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 30, 3);
    const uint8_t modules[] = {SW(1), SW(2)};
    settle(&f, modules, 2);

    uint8_t broken[MAX_MODULES + 1];
    memset(broken, 0x00, sizeof(broken));
    for (int i = 0; i < MAX_KEYS; i++) {
        f.active[i] = true;
    }
    CHECK_EQ(chain_scan(&f.chain, broken, sizeof(broken), f.active), CHAIN_SCAN_SETTLING);
    CHECK_EQ(f.chain.observed, CHAIN_COUNT_FAULT);
    CHECK_EQ(f.chain.accepted, 2);
    CHECK_EQ(active_total(&f), 5 * 30); /* untouched: no key states from a faulty read */

    /* A persistent fault is accepted like a count, so held keys get released. */
    CHECK_EQ(chain_scan(&f.chain, broken, sizeof(broken), f.active), CHAIN_SCAN_SETTLING);
    CHECK_EQ(chain_scan(&f.chain, broken, sizeof(broken), f.active), CHAIN_SCAN_REMAP);
    CHECK_EQ(f.chain.accepted, CHAIN_COUNT_FAULT);
    CHECK_EQ(chain_scan(&f.chain, broken, sizeof(broken), f.active), CHAIN_SCAN_FAULT);

    /* Recovery goes through the stabilizer again. */
    CHECK_EQ(scan(&f, modules, 2), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 2), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 2), CHAIN_SCAN_REMAP);
    CHECK_EQ(scan(&f, modules, 2), CHAIN_SCAN_KEYS);
}

static void test_flapping_during_insertion(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 30, 3);
    const uint8_t modules[] = {SW(1), SW(1), SW(1), SW(1)};
    settle(&f, modules, 2);
    CHECK_EQ(f.chain.accepted, 2);

    /* A third module sliding on: contacts bounce between 2, 3 and garbage. */
    uint8_t garbage[MAX_MODULES + 1];
    memset(garbage, 0x00, sizeof(garbage));
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(chain_scan(&f.chain, garbage, sizeof(garbage), f.active), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 2), CHAIN_SCAN_KEYS); /* back to the accepted count */
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(f.chain.accepted, 2);
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_REMAP);
    CHECK_EQ(f.chain.accepted, 3);
    CHECK_EQ(scan(&f, modules, 3), CHAIN_SCAN_KEYS);
    CHECK_EQ(active_total(&f), 3);
}

static void test_report_diff(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_MCU, 15, 3);
    bool pressed[MAX_KEYS] = {false};

    pressed[4 * 15 + 14] = true;
    chain_report(&f.chain, pressed, record, &f.rec);
    CHECK_EQ(f.rec.count, 1);
    CHECK_EQ(f.rec.events[0].row, 4);
    CHECK_EQ(f.rec.events[0].column, 14);
    CHECK(f.rec.events[0].pressed);

    chain_report(&f.chain, pressed, record, &f.rec);
    CHECK_EQ(f.rec.count, 1); /* no change, no event */

    pressed[4 * 15 + 14] = false;
    chain_report(&f.chain, pressed, record, &f.rec);
    CHECK_EQ(f.rec.count, 2);
    CHECK(!f.rec.events[1].pressed);
}

static void test_count_change_releases_held_keys(void) {
    struct fixture f;
    setup(&f, CHAIN_ANCHOR_TERMINATOR, 15, 3);
    const uint8_t two[] = {SW(1), SW(5)};
    CHECK_EQ(settle(&f, two, 2), CHAIN_SCAN_KEYS);
    chain_report(&f.chain, f.active, record, &f.rec);
    CHECK_EQ(f.rec.count, 2);

    /* A third module joins at the terminator end while both keys stay held. */
    const uint8_t three[] = {SW(1), SW(5), 0x00};
    CHECK_EQ(scan(&f, three, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, three, 3), CHAIN_SCAN_SETTLING);
    CHECK_EQ(scan(&f, three, 3), CHAIN_SCAN_REMAP);
    f.rec.count = 0;
    chain_release_all(&f.chain, record, &f.rec);
    CHECK_EQ(f.rec.count, 2);
    for (int i = 0; i < f.rec.count; i++) {
        CHECK(!f.rec.events[i].pressed);
    }
    chain_release_all(&f.chain, record, &f.rec);
    CHECK_EQ(f.rec.count, 2); /* nothing left to release */

    /* The held keys come back at their new keymap columns. */
    CHECK_EQ(scan(&f, three, 3), CHAIN_SCAN_KEYS);
    CHECK(active_at(&f, 4, 2));
    CHECK(active_at(&f, 0, 1));
    CHECK_EQ(active_total(&f), 2);
}

static void test_seven_rows(void) {
    struct fixture f;
    memset(&f, 0, sizeof(f));
    const struct chain_config config = {CHAIN_ANCHOR_MCU, 2, 7, 1};
    chain_init(&f.chain, &config, f.reported);
    const uint8_t modules[] = {0x40 /* G, SW7 */, 0x20 /* F, SW6 */};
    uint8_t buf[MAX_MODULES + 1];
    const size_t len = read_of(buf, modules, 2);
    CHECK_EQ(chain_scan(&f.chain, buf, len, f.active), CHAIN_SCAN_REMAP);
    CHECK_EQ(chain_scan(&f.chain, buf, len, f.active), CHAIN_SCAN_KEYS);
    CHECK(f.active[0 * 2 + 1]); /* SW7 on physical 0: row 0, column 1 */
    CHECK(f.active[1 * 2 + 0]); /* SW6 on physical 1: row 1, column 0 */
}

int main(void) {
    test_sentinel_search();
    test_zero_modules();
    test_one_module_rows();
    test_n_modules_anchor_mcu();
    test_n_modules_anchor_terminator();
    test_column_mapping();
    test_more_modules_than_columns();
    test_missing_sentinel();
    test_flapping_during_insertion();
    test_report_diff();
    test_count_change_releases_held_keys();
    test_seven_rows();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("chain_test: all checks passed\n");
    return 0;
}
