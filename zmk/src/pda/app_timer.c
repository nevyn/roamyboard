/*
 * Timer: a stopwatch, or a countdown of the minutes typed, with P for a 25-minute pomodoro
 * and B for a 5-minute break. It keeps counting while the keys go to the host.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "keys.h"
#include "shell.h"

/** Often enough that the view changes within a fifth of a second of the second changing. */
#define TIMER_TICK_MS 200

enum {
    USAGE_B = 0x05,
    USAGE_P = 0x13,
    USAGE_S = 0x16,
    USAGE_1 = 0x1E,
    USAGE_0 = 0x27,
    USAGE_SPACE = 0x2C,
    USAGE_KP_1 = 0x59,
    USAGE_KP_0 = 0x62,
};

static uint32_t elapsed_ms(const struct pda_shell *shell) {
    const struct pda_timer_state *t = &shell->timer;
    return t->elapsed_ms + (t->running ? shell->now_ms - t->started_ms : 0);
}

/** The seconds that the view shows: counted up, or left of the countdown, rounded up. */
static uint32_t shown_seconds(const struct pda_shell *shell) {
    const struct pda_timer_state *t = &shell->timer;
    const uint32_t ms = elapsed_ms(shell);
    if (t->countdown_s == 0) {
        return ms / 1000;
    }
    const uint32_t total = t->countdown_s * 1000;
    return ms >= total ? 0 : (total - ms + 999) / 1000;
}

static void set_tick(struct pda_shell *shell) {
    shell->tick_ms = shell->timer.running ? TIMER_TICK_MS : 0;
}

static void reset(struct pda_shell *shell) {
    struct pda_timer_state *t = &shell->timer;
    t->running = false;
    t->done = false;
    t->elapsed_ms = 0;
    set_tick(shell);
}

static void set_countdown(struct pda_shell *shell, uint32_t seconds) {
    shell->timer.countdown_s = seconds;
    reset(shell);
}

static void timer_open(struct pda_shell *shell) { set_tick(shell); }

static int digit_of(uint8_t usage) {
    if (usage >= USAGE_1 && usage <= USAGE_0) {
        return usage == USAGE_0 ? 0 : usage - USAGE_1 + 1;
    }
    if (usage >= USAGE_KP_1 && usage <= USAGE_KP_0) {
        return usage == USAGE_KP_0 ? 0 : usage - USAGE_KP_1 + 1;
    }
    return -1;
}

static enum pda_app_result timer_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                     bool pressed) {
    (void)implicit_mods;
    struct pda_timer_state *t = &shell->timer;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    const int digit = digit_of(usage);
    if (digit >= 0) {
        if (t->running) {
            return PDA_APP_NONE;
        }
        t->entry = (uint16_t)((t->entry * 10 + digit) % 1000);
        set_countdown(shell, t->entry * 60u);
        return PDA_APP_REDRAW;
    }
    switch (usage) {
    case PDA_USAGE_ESCAPE:
        return PDA_APP_CLOSE;
    case USAGE_SPACE:
    case PDA_USAGE_RETURN:
    case PDA_USAGE_KP_ENTER:
        t->entry = 0;
        if (t->done) {
            reset(shell);
        } else if (t->running) {
            t->elapsed_ms = elapsed_ms(shell);
            t->running = false;
        } else {
            t->started_ms = shell->now_ms;
            t->running = true;
        }
        set_tick(shell);
        return PDA_APP_REDRAW;
    case PDA_USAGE_BACKSPACE:
        if (t->entry > 0 && !t->running) {
            t->entry /= 10;
            set_countdown(shell, t->entry * 60u);
        } else {
            reset(shell);
        }
        return PDA_APP_REDRAW;
    case USAGE_P:
        t->entry = 0;
        set_countdown(shell, 25 * 60);
        return PDA_APP_REDRAW;
    case USAGE_B:
        t->entry = 0;
        set_countdown(shell, 5 * 60);
        return PDA_APP_REDRAW;
    case USAGE_S:
        t->entry = 0;
        set_countdown(shell, 0);
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result timer_tick(struct pda_shell *shell) {
    struct pda_timer_state *t = &shell->timer;
    if (!t->running) {
        return PDA_APP_NONE;
    }
    if (t->countdown_s && elapsed_ms(shell) >= t->countdown_s * 1000) {
        t->running = false;
        t->done = true;
        t->elapsed_ms = t->countdown_s * 1000;
        set_tick(shell);
        return PDA_APP_REDRAW;
    }
    const uint32_t shown = shown_seconds(shell);
    if (shown == t->shown_s) {
        return PDA_APP_NONE;
    }
    t->shown_s = shown;
    return PDA_APP_REDRAW;
}

static void timer_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_timer_state *t = &shell->timer;
    const uint32_t s = t->done ? 0 : shown_seconds(shell);

    char mode[16];
    if (t->done) {
        snprintf(mode, sizeof(mode), "done!");
    } else if (t->countdown_s) {
        snprintf(mode, sizeof(mode), "%lu min", (unsigned long)(t->countdown_s / 60));
    } else {
        snprintf(mode, sizeof(mode), "watch");
    }
    pda_text_center(fb, area, area.y + 4, mode, PDA_FONT_BODY, true);

    // Under an hour: minutes and seconds; from an hour on: hours and minutes.
    const int clock_y = area.y + 26;
    const bool hours = s >= 3600;
    if (t->done) {
        pda_fb_fill(fb, (struct pda_rect){area.x, clock_y - 4, area.w, PDA_SEGMENT_H + 8}, true);
    }
    pda_fb_seven_segment_pair(fb, area, clock_y, hours ? (int)(s / 3600) : (int)(s / 60),
                              hours ? (int)(s / 60 % 60) : (int)(s % 60), !t->done);

    char status[16];
    if (t->running) {
        snprintf(status, sizeof(status), "running");
    } else if (t->done) {
        snprintf(status, sizeof(status), "done");
    } else if (t->elapsed_ms) {
        snprintf(status, sizeof(status), "paused");
    } else {
        snprintf(status, sizeof(status), "%s", hours ? "h : min" : "ready");
    }
    pda_text_center(fb, area, clock_y + PDA_SEGMENT_H + 8, status, PDA_FONT_BODY, true);
}

static const char timer_help[] =
    "0 to 9\tType the countdown's minutes.\n"
    "P\t25 minutes: a pomodoro.\n"
    "B\t5 minutes: a break.\n"
    "S\tThe stopwatch.\n"
    "Space, Return\tStart or pause; again after it ends.\n"
    "Backspace\tBack to zero, or delete the last digit.\n"
    "Esc\tBack to the home screen; it keeps counting.";

const struct pda_app pda_app_timer_def = {
    .name = "Timer",
    .open = timer_open,
    .key = timer_key,
    .tick = timer_tick,
    .draw = timer_draw,
    .help = timer_help,
};
