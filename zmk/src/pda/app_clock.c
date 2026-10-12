/*
 * Clock: the time in large digits, with the seconds, the weekday, the date and the ISO week,
 * once a host has given the time (docs/pda.md, Clock).
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "keys.h"
#include "shell.h"

/** Often enough that the seconds change within a quarter second of the real ones. */
#define CLOCK_TICK_MS 250

static void clock_open(struct pda_shell *shell) {
    shell->tick_ms = CLOCK_TICK_MS;
    shell->clock_app.shown_s = UINT32_MAX;
}

static enum pda_app_result clock_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                     bool pressed) {
    (void)shell;
    (void)implicit_mods;
    return pressed && usage == PDA_USAGE_ESCAPE ? PDA_APP_CLOSE : PDA_APP_NONE;
}

static enum pda_app_result clock_tick(struct pda_shell *shell) {
    struct pda_date_time now;
    if (!pda_shell_time(shell, &now)) {
        return PDA_APP_NONE;
    }
    const uint32_t s = (uint32_t)(now.hour * 3600 + now.minute * 60 + now.second);
    if (s == shell->clock_app.shown_s) {
        return PDA_APP_NONE;
    }
    shell->clock_app.shown_s = s;
    return PDA_APP_REDRAW;
}

static void clock_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    struct pda_date_time now;
    if (!pda_shell_time(shell, &now)) {
        static const char *const lines[] = {"no time", "yet:", "connect", "a phone"};
        for (int i = 0; i < 4; i++) {
            pda_text_center(fb, area, area.y + 30 + i * pda_text_line_height(PDA_FONT_BODY),
                            lines[i], PDA_FONT_BODY, true);
        }
        return;
    }
    static const char *const weekdays[] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                                           "Friday", "Saturday", "Sunday"};
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const int digits_y = area.y + 14;
    pda_fb_seven_segment_pair(fb, area, digits_y, now.hour, now.minute, true);

    char text[24];
    snprintf(text, sizeof(text), ":%02d", now.second);
    pda_text_center(fb, area, digits_y + PDA_SEGMENT_H + 6, text, PDA_FONT_SMALL, true);

    const int line_h = pda_text_line_height(PDA_FONT_BODY);
    int y = digits_y + PDA_SEGMENT_H + 24;
    pda_text_center(fb, area, y, weekdays[now.weekday - 1], PDA_FONT_BODY, true);
    y += line_h;
    snprintf(text, sizeof(text), "%d %s", now.day, months[now.month - 1]);
    pda_text_center(fb, area, y, text, PDA_FONT_BODY, true);
    y += line_h;
    snprintf(text, sizeof(text), "%d", now.year);
    pda_text_center(fb, area, y, text, PDA_FONT_BODY, true);
    y += line_h + 6;
    snprintf(text, sizeof(text), "week %d", pda_clock_iso_week(now));
    pda_text_center(fb, area, y, text, PDA_FONT_BODY, true);
}

const struct pda_app pda_app_clock_def = {
    .name = "Clock",
    .open = clock_open,
    .key = clock_key,
    .tick = clock_tick,
    .draw = clock_draw,
};
