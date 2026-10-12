/*
 * SPDX-License-Identifier: MIT
 */

#include "clock.h"

/** Days since 2000-01-01: Howard Hinnant's days_from_civil, offset to 2000. */
static int32_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    const int era = y / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    // 730425: days from 0000-03-01 to 2000-01-01.
    return era * 146097 + doe - 730425;
}

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int days_in_month(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && leap(y) ? 29 : days[m - 1];
}

bool pda_clock_from_cts(const uint8_t *value, size_t len, uint32_t *seconds) {
    if (len < 7) {
        return false;
    }
    const int year = value[0] | value[1] << 8;
    const int month = value[2], day = value[3], hour = value[4], minute = value[5],
              second = value[6];
    // 2135 keeps the seconds within 32 bits.
    if (year < 2000 || year > 2135 || month < 1 || month > 12 || day < 1 ||
        day > days_in_month(year, month) || hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    *seconds = (uint32_t)days_from_civil(year, month, day) * 86400u +
               (uint32_t)(hour * 3600 + minute * 60 + second);
    return true;
}

/** Weekday, 1 for Monday to 7 for Sunday; days before 2000 count negative. */
static int weekday_of(int year, int month, int day) {
    const int32_t days = days_from_civil(year, month, day) + 5; // 2000-01-01 was a Saturday
    return (int)((days % 7 + 7) % 7) + 1;
}

static int weeks_in_year(int year) {
    // A year has 53 weeks if it starts on a Thursday, or on a Wednesday in a leap year.
    const int jan1 = weekday_of(year, 1, 1);
    return jan1 == 4 || (jan1 == 3 && leap(year)) ? 53 : 52;
}

int pda_clock_iso_week(struct pda_date_time t) {
    const int ordinal = (int)(days_from_civil(t.year, t.month, t.day) -
                              days_from_civil(t.year, 1, 1)) + 1;
    const int week = (ordinal - t.weekday + 10) / 7;
    if (week < 1) {
        return weeks_in_year(t.year - 1);
    }
    if (week > weeks_in_year(t.year)) {
        return 1;
    }
    return week;
}

struct pda_date_time pda_clock_split(uint32_t seconds) {
    struct pda_date_time t;
    uint32_t days = seconds / 86400u;
    const uint32_t rest = seconds % 86400u;
    t.hour = (int)(rest / 3600);
    t.minute = (int)(rest / 60 % 60);
    t.second = (int)(rest % 60);
    // 2000-01-01 was a Saturday.
    t.weekday = (int)((days + 5) % 7) + 1;
    t.year = 2000;
    while (days >= (uint32_t)(leap(t.year) ? 366 : 365)) {
        days -= leap(t.year) ? 366 : 365;
        t.year++;
    }
    t.month = 1;
    while (days >= (uint32_t)days_in_month(t.year, t.month)) {
        days -= (uint32_t)days_in_month(t.year, t.month);
        t.month++;
    }
    t.day = (int)days + 1;
    return t;
}
