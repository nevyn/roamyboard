/*
 * Helpers shared by the PDA's host tests: checks, a stub for LVGL's text, and driving the
 * shell by key.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "keys.h"
#include "shell.h"

extern int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

enum {
    USAGE_A = 0x04,
    USAGE_H = 0x0B,
    USAGE_I = 0x0C,
    USAGE_J = 0x0D,
    USAGE_N = 0x11,
    USAGE_S = 0x16,
    USAGE_1 = 0x1E,
    USAGE_2 = 0x1F,
    USAGE_3 = 0x20,
    USAGE_4 = 0x21,
    USAGE_5 = 0x22,
    USAGE_8 = 0x25,
    USAGE_SPACE = 0x2C,
    USAGE_MINUS = 0x2D,
    USAGE_EQUAL = 0x2E,
    USAGE_DOT = 0x37,
    USAGE_SLASH = 0x38,
    USAGE_RIGHT = 0x4F,
    USAGE_LEFT = 0x50,
    USAGE_DOWN = 0x51,
    USAGE_UP = 0x52,
    LSHIFT = 0xE1,
    LGUI = 0xE3,
};

static inline enum pda_shell_action tap(struct pda_shell *shell, uint8_t usage) {
    const enum pda_shell_action action = pda_shell_key(shell, usage, 0, true);
    pda_shell_key(shell, usage, 0, false);
    return action;
}

static inline enum pda_shell_action tap_mods(struct pda_shell *shell, uint8_t modifier,
                                             uint8_t usage) {
    pda_shell_key(shell, modifier, 0, true);
    const enum pda_shell_action action = tap(shell, usage);
    pda_shell_key(shell, modifier, 0, false);
    return action;
}

static inline void type(struct pda_shell *shell, const char *letters) {
    for (const char *c = letters; *c; c++) {
        tap(shell, (uint8_t)(USAGE_A + (*c - 'a')));
    }
}

static inline const char *app_name(const struct pda_shell *shell) {
    return shell->app == PDA_SHELL_HOME ? "home" : pda_apps[shell->app]->name;
}

static inline int app_index(const char *name) {
    for (int i = 0; i < pda_app_count; i++) {
        if (strcmp(pda_apps[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/** Focuses the PDA and opens the app from home: by its digit key, or with Down and Return. */
static inline void open_named(struct pda_shell *shell, const char *name) {
    pda_shell_set_mode(shell, ROAMYBOARD_PDA_HOME);
    while (shell->app != PDA_SHELL_HOME) {
        tap(shell, PDA_USAGE_ESCAPE);
    }
    const int i = app_index(name);
    CHECK(i >= 0);
    if (i < 10) {
        tap(shell, (uint8_t)(USAGE_1 + i)); // USAGE_1 + 9 is the 0 key
        return;
    }
    while (shell->selected != i) {
        tap(shell, USAGE_RIGHT);
    }
    tap(shell, PDA_USAGE_RETURN);
}

/** Advances time by ms in ticks of the open app's tick_ms, as the platform would. */
static inline void run_ms(struct pda_shell *shell, uint32_t ms) {
    const uint32_t end = shell->now_ms + ms;
    while ((int32_t)(end - shell->now_ms) > 0) {
        const uint32_t step = pda_shell_tick_ms(shell) ? pda_shell_tick_ms(shell) : ms;
        shell->now_ms += step;
        pda_shell_tick(shell);
    }
}
