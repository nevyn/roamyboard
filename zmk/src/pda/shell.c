/*
 * SPDX-License-Identifier: MIT
 */

#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "icons.h"
#include "keys.h"
#include "midi_view.h"

const struct pda_app *const pda_apps[] = {
    &pda_app_note_def,   &pda_app_clock_def, &pda_app_timer_def,    &pda_app_calc_def,
    &pda_app_dice_def,   &pda_app_kana_def,  &pda_app_tetris_def,   &pda_app_snake_def,
    &pda_app_breakout_def,
    &pda_app_words_def,  &pda_app_rain_def,  &pda_app_keys_def,     &pda_app_info_def,
};
const int pda_app_count = sizeof(pda_apps) / sizeof(pda_apps[0]);
const int pda_app_note = 0;

enum {
    USAGE_A = 0x04,
    USAGE_D = 0x07,
    USAGE_H = 0x0B,
    USAGE_J = 0x0D,
    USAGE_L = 0x0F,
    USAGE_S = 0x16,
    USAGE_W = 0x1A,
    USAGE_1 = 0x1E,
    USAGE_0 = 0x27,
    USAGE_SPACE = 0x2C,
    USAGE_RIGHT = 0x4F,
    USAGE_LEFT = 0x50,
    USAGE_DOWN = 0x51,
    USAGE_UP = 0x52,
    USAGE_KP_1 = 0x59,
    USAGE_KP_0 = 0x62,
};

enum pda_dir pda_key_dir(uint8_t usage) {
    switch (usage) {
    case USAGE_UP:
    case USAGE_W:
    case PDA_USAGE_K:
        return PDA_DIR_UP;
    case USAGE_DOWN:
    case USAGE_S:
    case USAGE_J:
        return PDA_DIR_DOWN;
    case USAGE_LEFT:
    case USAGE_A:
    case USAGE_H:
        return PDA_DIR_LEFT;
    case USAGE_RIGHT:
    case USAGE_D:
    case USAGE_L:
        return PDA_DIR_RIGHT;
    }
    return PDA_DIR_NONE;
}

void pda_shell_init(struct pda_shell *shell, uint32_t seed) {
    memset(shell, 0, sizeof(*shell));
    shell->app = PDA_SHELL_HOME;
    shell->before_quick = PDA_SHELL_HOME;
    shell->last_app = PDA_SHELL_HOME;
    // xorshift32 never leaves 0, so 0 is not a seed.
    shell->random_state = seed ? seed : 0x9E3779B9u;
    pda_note_init(&shell->note);
    shell->keys.last_position = -1;
    shell->keys.last_usage = -1;
    shell->midi.last_note = -1;
}

uint32_t pda_shell_random(struct pda_shell *shell, uint32_t n) {
    uint32_t x = shell->random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    shell->random_state = x;
    return x % n;
}

static void open_app(struct pda_shell *shell, int app) {
    shell->app = app;
    shell->selected = app;
    shell->tick_ms = 0;
    if (pda_apps[app]->open) {
        pda_apps[app]->open(shell);
    }
}

static void go_home(struct pda_shell *shell) {
    shell->app = PDA_SHELL_HOME;
    shell->tick_ms = 0;
}

bool pda_shell_show(struct pda_shell *shell, const char *name) {
    if (name[0] == '\0') {
        go_home(shell);
        return true;
    }
    for (int i = 0; i < pda_app_count; i++) {
        if (strcmp(pda_apps[i]->name, name) == 0) {
            open_app(shell, i);
            shell->last_app = i;
            return true;
        }
    }
    return false;
}

const char *pda_shell_last_app_name(const struct pda_shell *shell) {
    return shell->last_app == PDA_SHELL_HOME ? "" : pda_apps[shell->last_app]->name;
}

/** Opens an app that the home screen chose: it becomes the last app. */
static void open_from_home(struct pda_shell *shell, int app) {
    open_app(shell, app);
    shell->last_app = app;
}

/** Ends quick note: back to the app that it replaced, which opens again. */
static void end_quick(struct pda_shell *shell) {
    if (!shell->quick) {
        return;
    }
    shell->quick = false;
    if (shell->before_quick == PDA_SHELL_HOME) {
        go_home(shell);
    } else {
        open_app(shell, shell->before_quick);
    }
}

enum pda_shell_action pda_shell_set_mode(struct pda_shell *shell, enum roamyboard_pda_mode mode) {
    pda_note_reset_keys(&shell->note);
    shell->held_mods = 0;
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

/** The app that a digit key opens, or -1: 1 to 9 open the first nine, 0 the tenth. */
static int app_for_digit(uint8_t usage) {
    int n = -1;
    if (usage >= USAGE_1 && usage <= USAGE_0) {
        n = usage - USAGE_1;
    } else if (usage >= USAGE_KP_1 && usage <= USAGE_KP_0) {
        n = usage - USAGE_KP_1;
    }
    return n < pda_app_count ? n : -1;
}

static enum pda_shell_action home_key(struct pda_shell *shell, uint8_t usage) {
    const int digit_app = app_for_digit(usage);
    if (digit_app >= 0) {
        open_from_home(shell, digit_app);
        return PDA_SHELL_REDRAW;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_SHELL_LEAVE;
    }
    if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE) {
        open_from_home(shell, shell->selected);
        return PDA_SHELL_REDRAW;
    }
    const int selected = shell->selected;
    switch (pda_key_dir(usage)) {
    case PDA_DIR_RIGHT:
        shell->selected = (selected + 1) % pda_app_count;
        break;
    case PDA_DIR_LEFT:
        shell->selected = (selected + pda_app_count - 1) % pda_app_count;
        break;
    case PDA_DIR_DOWN:
        if (selected + PDA_HOME_COLUMNS < pda_app_count) {
            shell->selected = selected + PDA_HOME_COLUMNS;
        }
        break;
    case PDA_DIR_UP:
        if (selected >= PDA_HOME_COLUMNS) {
            shell->selected = selected - PDA_HOME_COLUMNS;
        }
        break;
    case PDA_DIR_NONE:
        break;
    }
    return shell->selected != selected ? PDA_SHELL_REDRAW : PDA_SHELL_NONE;
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
        go_home(shell);
        return PDA_SHELL_REDRAW;
    case PDA_APP_TYPE_OUT:
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
    if (usage >= PDA_USAGE_LEFT_CONTROL && usage <= PDA_USAGE_RIGHT_GUI) {
        const uint8_t bit = (uint8_t)(1u << (usage - PDA_USAGE_LEFT_CONTROL));
        shell->held_mods = pressed ? (shell->held_mods | bit) : (shell->held_mods & ~bit);
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

uint32_t pda_shell_tick_ms(const struct pda_shell *shell) {
    if (shell->app == PDA_SHELL_HOME || pda_apps[shell->app]->tick == NULL ||
        (pda_apps[shell->app]->focused_ticks && shell->mode == ROAMYBOARD_PDA_OFF)) {
        return 0;
    }
    return shell->tick_ms;
}

enum pda_shell_action pda_shell_tick(struct pda_shell *shell) {
    if (pda_shell_tick_ms(shell) == 0) {
        return PDA_SHELL_NONE;
    }
    return app_result(shell, pda_apps[shell->app]->tick(shell));
}

void pda_text_center(struct pda_fb *fb, struct pda_rect area, int y, const char *text,
                     enum pda_font font, bool lit) {
    const int w = pda_text_width(text, font);
    const int x = area.x + (w < area.w ? (area.w - w) / 2 : 0);
    pda_text_draw(fb, x, y, area.w - (x - area.x), text, font, lit);
}

void pda_game_banner(struct pda_fb *fb, struct pda_rect area, int y, const char *const lines[3]) {
    const int line_h = pda_text_line_height(PDA_FONT_BODY);
    int count = 0;
    while (count < 3 && lines[count]) {
        count++;
    }
    const struct pda_rect banner = {area.x + 2, y, area.w - 4, count * line_h + 6};
    pda_fb_fill(fb, banner, false);
    pda_fb_frame(fb, banner, true);
    for (int i = 0; i < count; i++) {
        pda_text_center(fb, area, y + 3 + i * line_h, lines[i], PDA_FONT_BODY, true);
    }
}

static void draw_battery(struct pda_fb *fb, uint8_t percent) {
    const struct pda_rect body = {PDA_FB_W - 14, 6, 11, 8};
    pda_fb_frame(fb, body, true);
    pda_fb_fill(fb, (struct pda_rect){body.x + body.w, body.y + 2, 2, 4}, true);
    const int fill = (body.w - 4) * (percent > 100 ? 100 : percent) / 100;
    pda_fb_fill(fb, (struct pda_rect){body.x + 2, body.y + 2, fill, body.h - 4}, true);
}

static const struct pda_icon *icon_for(const struct pda_app *app) {
    for (int i = 0; i < pda_icon_count; i++) {
        const char *a = app->name, *b = pda_icons[i].name;
        while (*a && tolower((unsigned char)*a) == *b) {
            a++, b++;
        }
        if (*a == '\0' && *b == '\0') {
            return &pda_icons[i];
        }
    }
    return NULL;
}

static void draw_icon(struct pda_fb *fb, int x, int y, const struct pda_icon *icon, bool lit) {
    for (int row = 0; row < PDA_ICON_SIZE; row++) {
        for (int col = 0; col < PDA_ICON_SIZE; col++) {
            if (icon->rows[row] & (0x8000 >> col)) {
                pda_fb_set(fb, x + col, y + row, lit);
            }
        }
    }
}

#define HOME_CELL 22

static void draw_home(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const int name_h = pda_text_line_height(PDA_FONT_BODY) + 3;
    const int visible = (area.h - 2 - name_h) / HOME_CELL;
    const int rows = (pda_app_count + PDA_HOME_COLUMNS - 1) / PDA_HOME_COLUMNS;
    const int selected_row = shell->selected / PDA_HOME_COLUMNS;
    // Scrolls by rows, just enough to keep the highlight in view.
    const int first = selected_row >= visible ? selected_row - visible + 1 : 0;
    const int x0 = area.x + (area.w - PDA_HOME_COLUMNS * HOME_CELL) / 2;
    const int pad = (HOME_CELL - PDA_ICON_SIZE) / 2;

    for (int i = first * PDA_HOME_COLUMNS; i < pda_app_count && i / PDA_HOME_COLUMNS < first + visible;
         i++) {
        const int x = x0 + i % PDA_HOME_COLUMNS * HOME_CELL;
        const int y = area.y + 2 + (i / PDA_HOME_COLUMNS - first) * HOME_CELL;
        const bool selected = i == shell->selected;
        if (selected) {
            pda_fb_fill(fb, (struct pda_rect){x, y, HOME_CELL, HOME_CELL}, true);
        }
        const struct pda_icon *icon = icon_for(pda_apps[i]);
        if (icon) {
            draw_icon(fb, x + pad, y + pad, icon, !selected);
        } else {
            pda_fb_frame(fb, (struct pda_rect){x + pad, y + pad, PDA_ICON_SIZE, PDA_ICON_SIZE},
                         !selected);
        }
    }
    // Marks on the right edge show that the grid goes on above or below.
    if (first > 0) {
        pda_fb_fill(fb, (struct pda_rect){area.x + area.w - 2, area.y + 2, 2, 2}, true);
    }
    if (first + visible < rows) {
        pda_fb_fill(fb, (struct pda_rect){area.x + area.w - 2, area.y + 2 + visible * HOME_CELL - 2,
                                          2, 2},
                    true);
    }

    char label[24];
    if (shell->selected < 10) {
        snprintf(label, sizeof(label), "%d %s", (shell->selected + 1) % 10,
                 pda_apps[shell->selected]->name);
    } else {
        snprintf(label, sizeof(label), "%s", pda_apps[shell->selected]->name);
    }
    pda_text_center(fb, area, area.y + 4 + visible * HOME_CELL, label, PDA_FONT_BODY, true);
}

bool pda_shell_time(const struct pda_shell *shell, struct pda_date_time *out) {
    if (shell->clock.status != PDA_CLOCK_SET) {
        return false;
    }
    *out = pda_clock_split(shell->clock.seconds + (shell->now_ms - shell->clock.at_ms) / 1000);
    return true;
}

enum pda_shell_action pda_shell_midi(struct pda_shell *shell, const struct pda_midi_view *view) {
    const bool visible = shell->midi.shown || view->shown;
    shell->midi = *view;
    return visible && shell->mode == ROAMYBOARD_PDA_OFF ? PDA_SHELL_REDRAW : PDA_SHELL_NONE;
}

void pda_shell_draw(const struct pda_shell *shell, struct pda_fb *fb) {
    pda_fb_clear(fb);
    const struct pda_rect area = {0, PDA_TITLE_H, PDA_FB_W, PDA_FB_H - PDA_TITLE_H};
    if (shell->midi.shown && shell->mode == ROAMYBOARD_PDA_OFF) {
        pda_text_draw(fb, 2, 2, PDA_FB_W - 17, "MIDI", PDA_FONT_TITLE, true);
        draw_battery(fb, shell->right.battery);
        pda_fb_fill(fb, (struct pda_rect){0, PDA_TITLE_H - 2, PDA_FB_W, 1}, true);
        pda_midi_view_draw(&shell->midi, fb, area);
        return;
    }
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
    pda_text_draw(fb, 2, 2, PDA_FB_W - 17, title, PDA_FONT_TITLE, true);
    draw_battery(fb, shell->right.battery);
    pda_fb_fill(fb, (struct pda_rect){0, PDA_TITLE_H - 2, PDA_FB_W, 1}, true);

    if (shell->app == PDA_SHELL_HOME) {
        draw_home(shell, fb, area);
    } else {
        pda_apps[shell->app]->draw(shell, fb, area);
    }
}
