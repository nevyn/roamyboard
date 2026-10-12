/*
 * Host tests for zmk/src/pda/shell.c, apps.c and gfx.c, with a stub for the text that
 * zmk/src/display/pda_text.c draws with LVGL. Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx.h"
#include "keys.h"
#include "shell.h"

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

/* Stub text: every byte is 6 px wide; draw marks nothing. */
int pda_text_line_height(enum pda_font font) { return font == PDA_FONT_SMALL ? 8 : 15; }
int pda_text_width(const char *text, enum pda_font font) {
    (void)font;
    return 6 * (int)strlen(text);
}
size_t pda_text_line_len(const char *text, size_t len, enum pda_font font, int max_w) {
    (void)font;
    size_t n = 0;
    while (n < len && (int)(n + 1) * 6 <= max_w) {
        if (text[n++] == '\n') {
            break;
        }
    }
    return n ? n : 1;
}
void pda_text_draw(struct pda_fb *fb, int x, int y, int max_w, const char *text,
                   enum pda_font font, bool lit) {
    (void)fb, (void)x, (void)y, (void)max_w, (void)text, (void)font, (void)lit;
}

enum {
    USAGE_J = 0x0D,
    USAGE_1 = 0x1E,
    USAGE_2 = 0x1F,
    USAGE_3 = 0x20,
    USAGE_DOWN = 0x51,
    USAGE_UP = 0x52,
    USAGE_H = 0x0B,
    USAGE_I = 0x0C,
    LGUI = 0xE3,
};

static enum pda_shell_action tap(struct pda_shell *shell, uint8_t usage) {
    const enum pda_shell_action action = pda_shell_key(shell, usage, 0, true);
    pda_shell_key(shell, usage, 0, false);
    return action;
}

static const char *app_name(const struct pda_shell *shell) {
    return shell->app == PDA_SHELL_HOME ? "home" : pda_apps[shell->app]->name;
}

static void test_keys_go_nowhere_without_focus(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    CHECK(tap(&shell, USAGE_2) == PDA_SHELL_NONE);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_home_navigation(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    CHECK(pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME) == PDA_SHELL_REDRAW);
    CHECK(tap(&shell, USAGE_J) == PDA_SHELL_REDRAW);
    CHECK(shell.selected == 1);
    tap(&shell, USAGE_DOWN);
    CHECK(shell.selected == 2);
    tap(&shell, USAGE_DOWN);
    CHECK(shell.selected == 0);
    tap(&shell, USAGE_UP);
    CHECK(shell.selected == pda_app_count - 1);
    tap(&shell, PDA_USAGE_K);
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(strcmp(app_name(&shell), "Keys") == 0);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_REDRAW);
    CHECK(shell.app == PDA_SHELL_HOME);
    CHECK(tap(&shell, USAGE_3) == PDA_SHELL_REDRAW);
    CHECK(strcmp(app_name(&shell), "Info") == 0);
    tap(&shell, PDA_USAGE_ESCAPE);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_LEAVE);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_leaving_focus_keeps_the_view(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    tap(&shell, USAGE_2);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(strcmp(app_name(&shell), "Keys") == 0);
    // Focusing again resumes the same app.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(strcmp(app_name(&shell), "Keys") == 0);
}

static void test_quick_note_returns_to_the_view_it_replaced(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    tap(&shell, USAGE_3);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);

    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    CHECK(strcmp(app_name(&shell), "Note") == 0);
    tap(&shell, USAGE_H);
    tap(&shell, USAGE_I);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_LEAVE);
    CHECK(strcmp(app_name(&shell), "Info") == 0);
    CHECK(strcmp(shell.note.text, "hi") == 0);

    // Cmd-Return types the note out and also returns.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    pda_shell_key(&shell, LGUI, 0, true);
    CHECK(tap(&shell, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(shell.type_out_len == 2 && memcmp(shell.type_out, "hi", 2) == 0);
    CHECK(shell.note.len == 0);
    CHECK(strcmp(app_name(&shell), "Info") == 0);
}

static void test_slot_6_during_quick_note_keeps_note_open(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(shell.app == PDA_SHELL_HOME);
    CHECK(!shell.quick);
}

static void test_note_opened_from_home_stays_after_type_out(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    tap(&shell, USAGE_1);
    tap(&shell, USAGE_H);
    pda_shell_key(&shell, LGUI, 0, true);
    CHECK(tap(&shell, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(strcmp(app_name(&shell), "Note") == 0);
    pda_shell_key(&shell, LGUI, 0, false);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_REDRAW);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_keys_app_tracks_positions(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_NONE); // not focused
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_NONE); // home has no positions
    tap(&shell, USAGE_2);
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_REDRAW);
    CHECK(shell.keys.held[30 / 8] & (1 << (30 % 8)));
    CHECK(shell.keys.last_position == 30);
    pda_shell_position(&shell, 30, false);
    CHECK(!(shell.keys.held[30 / 8] & (1 << (30 % 8))));
    CHECK(shell.keys.seen[30 / 8] & (1 << (30 % 8)));
    CHECK(pda_shell_position(&shell, 200, true) == PDA_SHELL_NONE);
}

static void test_home_draws_a_band_on_the_selected_app(void) {
    struct pda_shell shell;
    pda_shell_init(&shell);
    shell.selected = 1;
    static struct pda_fb fb;
    pda_shell_draw(&shell, &fb);
    const int row_h = pda_text_line_height(PDA_FONT_BODY) + 3;
    const int band_y = PDA_TITLE_H + 2 + row_h + row_h / 2;
    CHECK(pda_fb_get(&fb, PDA_FB_W / 2, band_y));
    CHECK(!pda_fb_get(&fb, PDA_FB_W / 2, PDA_TITLE_H + 2 + row_h / 2));
    // The rule under the title.
    CHECK(pda_fb_get(&fb, 0, PDA_TITLE_H - 2));
}

static void test_fb_clips(void) {
    static struct pda_fb fb;
    pda_fb_clear(&fb);
    pda_fb_fill(&fb, (struct pda_rect){-5, PDA_FB_H - 2, 10, 10}, true);
    CHECK(pda_fb_get(&fb, 0, PDA_FB_H - 1));
    CHECK(pda_fb_get(&fb, 4, PDA_FB_H - 2));
    CHECK(!pda_fb_get(&fb, 5, PDA_FB_H - 1));
    pda_fb_set(&fb, PDA_FB_W, 0, true);
    CHECK(!pda_fb_get(&fb, PDA_FB_W, 0));
    pda_fb_invert(&fb, (struct pda_rect){0, PDA_FB_H - 1, 1, 1});
    CHECK(!pda_fb_get(&fb, 0, PDA_FB_H - 1));
}

int main(void) {
    test_keys_go_nowhere_without_focus();
    test_home_navigation();
    test_leaving_focus_keeps_the_view();
    test_quick_note_returns_to_the_view_it_replaced();
    test_slot_6_during_quick_note_keeps_note_open();
    test_note_opened_from_home_stays_after_type_out();
    test_keys_app_tracks_positions();
    test_home_draws_a_band_on_the_selected_app();
    test_fb_clips();
    if (failures) {
        fprintf(stderr, "pda_shell_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_shell_test: all checks passed\n");
    return 0;
}
