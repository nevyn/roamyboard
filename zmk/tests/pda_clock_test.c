/*
 * Host tests for zmk/src/pda/clock.c. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "clock.h"

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/** A Current Time value with fractions and adjust reason, as a host sends it. */
static void cts(uint8_t out[10], int y, int mo, int d, int h, int mi, int s) {
    const uint8_t v[10] = {(uint8_t)(y & 0xFF), (uint8_t)(y >> 8), (uint8_t)mo, (uint8_t)d,
                           (uint8_t)h, (uint8_t)mi, (uint8_t)s, 0, 0x80, 0};
    for (int i = 0; i < 10; i++) {
        out[i] = v[i];
    }
}

static void check_round_trip(int y, int mo, int d, int h, int mi, int s, int weekday) {
    uint8_t v[10];
    cts(v, y, mo, d, h, mi, s);
    uint32_t seconds = 0;
    CHECK(pda_clock_from_cts(v, sizeof(v), &seconds));
    const struct pda_date_time t = pda_clock_split(seconds);
    CHECK(t.year == y && t.month == mo && t.day == d);
    CHECK(t.hour == h && t.minute == mi && t.second == s);
    CHECK(t.weekday == weekday);
}

static void test_round_trips(void) {
    check_round_trip(2000, 1, 1, 0, 0, 0, 6);    // Saturday
    check_round_trip(2000, 2, 29, 12, 0, 0, 2);  // Tuesday, leap day
    check_round_trip(2024, 12, 31, 23, 59, 59, 2);
    check_round_trip(2026, 10, 12, 21, 5, 30, 1); // Monday
    check_round_trip(2100, 3, 1, 6, 30, 0, 1);    // 2100 is no leap year
    check_round_trip(2135, 12, 31, 23, 59, 59, 6);
}

static void test_epoch_is_zero(void) {
    uint8_t v[10];
    cts(v, 2000, 1, 1, 0, 0, 0);
    uint32_t seconds = 1;
    CHECK(pda_clock_from_cts(v, sizeof(v), &seconds));
    CHECK(seconds == 0);
    cts(v, 2000, 1, 2, 0, 0, 1);
    CHECK(pda_clock_from_cts(v, sizeof(v), &seconds));
    CHECK(seconds == 86401);
}

static void test_rejects_bad_values(void) {
    uint8_t v[10];
    uint32_t seconds;
    cts(v, 2023, 2, 29, 0, 0, 0);
    CHECK(!pda_clock_from_cts(v, sizeof(v), &seconds));
    cts(v, 1999, 12, 31, 0, 0, 0);
    CHECK(!pda_clock_from_cts(v, sizeof(v), &seconds));
    cts(v, 2026, 0, 1, 0, 0, 0); // month 0: unknown in CTS
    CHECK(!pda_clock_from_cts(v, sizeof(v), &seconds));
    cts(v, 2026, 1, 1, 24, 0, 0);
    CHECK(!pda_clock_from_cts(v, sizeof(v), &seconds));
    cts(v, 2026, 1, 1, 0, 0, 0);
    CHECK(!pda_clock_from_cts(v, 6, &seconds));
    CHECK(pda_clock_from_cts(v, 7, &seconds));
}

static int week_of(int y, int mo, int d) {
    uint8_t v[10];
    cts(v, y, mo, d, 12, 0, 0);
    uint32_t seconds = 0;
    CHECK(pda_clock_from_cts(v, sizeof(v), &seconds));
    return pda_clock_iso_week(pda_clock_split(seconds));
}

static void test_iso_weeks(void) {
    CHECK(week_of(2026, 10, 12) == 42);
    CHECK(week_of(2026, 1, 1) == 1);   // a Thursday
    CHECK(week_of(2021, 1, 1) == 53);  // a Friday: the last week of 2020
    CHECK(week_of(2020, 12, 31) == 53);
    CHECK(week_of(2024, 12, 30) == 1); // a Monday in 2025's first week
    CHECK(week_of(2027, 1, 3) == 53);  // a Sunday: 2026 has 53 weeks
    CHECK(week_of(2000, 1, 1) == 52);  // a Saturday: the last week of 1999
}

int main(void) {
    test_iso_weeks();
    test_round_trips();
    test_epoch_is_zero();
    test_rejects_bad_values();
    if (failures) {
        fprintf(stderr, "pda_clock_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_clock_test: all checks passed\n");
    return 0;
}
