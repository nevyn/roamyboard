/*
 * SPDX-License-Identifier: MIT
 */

#include "shell.h"

#include <stdio.h>
#include <string.h>

#include "keys.h"

extern const struct pda_app pda_app_note_def;
extern const struct pda_app pda_app_keys_def;
extern const struct pda_app pda_app_info_def;

const struct pda_app *const pda_apps[] = {&pda_app_note_def, &pda_app_keys_def,
                                          &pda_app_info_def};
const int pda_app_count = sizeof(pda_apps) / sizeof(pda_apps[0]);
const int pda_app_note = 0;

enum {
    USAGE_J = 0x0D,
    USAGE_1 = 0x1E,
    USAGE_RIGHT = 0x4F,
    USAGE_DOWN = 0x51,
    USAGE_UP = 0x52,
    USAGE_KP_1 = 0x59,
};

void pda_shell_init(struct pda_shell *shell) {
    memset(shell, 0, sizeof(*shell));
    shell->app = PDA_SHELL_HOME;
    shell->before_quick = PDA_SHELL_HOME;
    pda_note_init(&shell->note);
    shell->keys.last_position = -1;
    shell->keys.last_usage = -1;
}

static void open_app(struct pda_shell *shell, int app) {
    shell->app = app;
    shell->selected = app;
    if (pda_apps[app]->open) {
        pda_apps[app]->open(shell);
    }
}

/** Ends quick note: back to the app that it replaced. */
static void end_quick(struct pda_shell *shell) {
    if (!shell->quick) {
        return;
    }
    shell->quick = false;
    shell->app = shell->before_quick;
    if (shell->app != PDA_SHELL_HOME) {
        shell->selected = shell->app;
    }
}

enum pda_shell_action pda_shell_set_mode(struct pda_shell *shell, enum roamyboard_pda_mode mode) {
    pda_note_reset_keys(&shell->note);
    if (mode == shell->mode) {
        return PDA_SHELL_NONE;
    }
    shell->mode = mode;
    switch (mode) {
    case ROAMYBOARD_PDA_NOTE:
        if (!shell->quick) {
            shell->before_quick = shell->app;
            shell->quick = true;
        }
        open_app(shell, pda_app_note);
        break;
    case ROAMYBOARD_PDA_HOME:
    case ROAMYBOARD_PDA_OFF:
        end_quick(shell);
        break;
    }
    return PDA_SHELL_REDRAW;
}

/** The app that a digit key opens, or -1. */
static int app_for_digit(uint8_t usage) {
    int n = -1;
    if (usage >= USAGE_1 && usage < USAGE_1 + 9) {
        n = usage - USAGE_1;
    } else if (usage >= USAGE_KP_1 && usage < USAGE_KP_1 + 9) {
        n = usage - USAGE_KP_1;
    }
    return n < pda_app_count ? n : -1;
}

static enum pda_shell_action home_key(struct pda_shell *shell, uint8_t usage) {
    const int digit_app = app_for_digit(usage);
    if (digit_app >= 0) {
        open_app(shell, digit_app);
        return PDA_SHELL_REDRAW;
    }
    switch (usage) {
    case PDA_USAGE_ESCAPE:
        return PDA_SHELL_LEAVE;
    case USAGE_J:
    case USAGE_DOWN:
        shell->selected = (shell->selected + 1) % pda_app_count;
        return PDA_SHELL_REDRAW;
    case PDA_USAGE_K:
    case USAGE_UP:
        shell->selected = (shell->selected + pda_app_count - 1) % pda_app_count;
        return PDA_SHELL_REDRAW;
    case PDA_USAGE_RETURN:
    case PDA_USAGE_KP_ENTER:
    case USAGE_RIGHT:
        open_app(shell, shell->selected);
        return PDA_SHELL_REDRAW;
    }
    return PDA_SHELL_NONE;
}

static enum pda_shell_action app_result(struct pda_shell *shell, enum pda_app_result result) {
    switch (result) {
    case PDA_APP_NONE:
        return PDA_SHELL_NONE;
    case PDA_APP_REDRAW:
        return PDA_SHELL_REDRAW;
    case PDA_APP_CLOSE:
        if (shell->quick) {
            end_quick(shell);
            return PDA_SHELL_LEAVE;
        }
        shell->app = PDA_SHELL_HOME;
        return PDA_SHELL_REDRAW;
    case PDA_APP_TYPE_OUT:
        shell->type_out_len = shell->note.len;
        memcpy(shell->type_out, shell->note.text, shell->note.len);
        pda_note_clear(&shell->note);
        end_quick(shell);
        return PDA_SHELL_TYPE_OUT;
    }
    return PDA_SHELL_NONE;
}

enum pda_shell_action pda_shell_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    if (shell->mode == ROAMYBOARD_PDA_OFF) {
        return PDA_SHELL_NONE;
    }
    if (shell->app != PDA_SHELL_HOME) {
        return app_result(shell, pda_apps[shell->app]->key(shell, usage, implicit_mods, pressed));
    }
    return pressed ? home_key(shell, usage) : PDA_SHELL_NONE;
}

enum pda_shell_action pda_shell_position(struct pda_shell *shell, uint8_t position, bool pressed) {
    if (shell->mode == ROAMYBOARD_PDA_OFF || shell->app == PDA_SHELL_HOME ||
        pda_apps[shell->app]->position == NULL) {
        return PDA_SHELL_NONE;
    }
    return app_result(shell, pda_apps[shell->app]->position(shell, position, pressed));
}

static void draw_battery(struct pda_fb *fb, uint8_t percent) {
    const struct pda_rect body = {PDA_FB_W - 17, 6, 14, 8};
    pda_fb_frame(fb, body, true);
    pda_fb_fill(fb, (struct pda_rect){body.x + body.w, body.y + 2, 2, 4}, true);
    const int fill = (body.w - 4) * (percent > 100 ? 100 : percent) / 100;
    pda_fb_fill(fb, (struct pda_rect){body.x + 2, body.y + 2, fill, body.h - 4}, true);
}

static void draw_home(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const int row_h = pda_text_line_height(PDA_FONT_BODY) + 3;
    for (int i = 0; i < pda_app_count; i++) {
        const int y = area.y + 2 + i * row_h;
        const bool selected = i == shell->selected;
        if (selected) {
            pda_fb_fill(fb, (struct pda_rect){area.x, y, area.w, row_h}, true);
        }
        char label[24];
        snprintf(label, sizeof(label), "%d %s", i + 1, pda_apps[i]->name);
        pda_text_draw(fb, area.x + 3, y + 1, area.w - 3, label, PDA_FONT_BODY, !selected);
    }
}

bool pda_shell_time(const struct pda_shell *shell, struct pda_date_time *out) {
    if (shell->clock.status != PDA_CLOCK_SET) {
        return false;
    }
    *out = pda_clock_split(shell->clock.seconds + (shell->now_ms - shell->clock.at_ms) / 1000);
    return true;
}

void pda_shell_draw(const struct pda_shell *shell, struct pda_fb *fb) {
    pda_fb_clear(fb);
    // Home shows the time, once there is one, where apps show their name.
    char clock[8];
    struct pda_date_time now;
    const char *title;
    if (shell->app != PDA_SHELL_HOME) {
        title = pda_apps[shell->app]->name;
    } else if (pda_shell_time(shell, &now)) {
        snprintf(clock, sizeof(clock), "%02d:%02d", now.hour, now.minute);
        title = clock;
    } else {
        title = "PDA";
    }
    pda_text_draw(fb, 2, 2, PDA_FB_W - 22, title, PDA_FONT_TITLE, true);
    draw_battery(fb, shell->right.battery);
    pda_fb_fill(fb, (struct pda_rect){0, PDA_TITLE_H - 2, PDA_FB_W, 1}, true);

    const struct pda_rect area = {0, PDA_TITLE_H, PDA_FB_W, PDA_FB_H - PDA_TITLE_H};
    if (shell->app == PDA_SHELL_HOME) {
        draw_home(shell, fb, area);
    } else {
        pda_apps[shell->app]->draw(shell, fb, area);
    }
}
