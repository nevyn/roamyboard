/*
 * Host tests for zmk/src/pda/shell.c, the small apps in apps.c, and gfx.c. Run with
 * zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pda_test_support.h"

static void test_keys_go_nowhere_without_focus(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    CHECK(tap(&shell, USAGE_2) == PDA_SHELL_NONE);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_zero_opens_the_tenth_app(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(tap(&shell, 0x27) == PDA_SHELL_REDRAW); // 0
    CHECK(shell.app == 9);
}

static void test_home_navigation(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    CHECK(pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME) == PDA_SHELL_REDRAW);
    CHECK(tap(&shell, USAGE_J) == PDA_SHELL_REDRAW);
    CHECK(shell.selected == 1);
    tap(&shell, USAGE_DOWN);
    CHECK(shell.selected == 2);
    tap(&shell, USAGE_UP);
    tap(&shell, PDA_USAGE_K);
    tap(&shell, PDA_USAGE_K);
    CHECK(shell.selected == pda_app_count - 1);
    tap(&shell, USAGE_DOWN);
    CHECK(shell.selected == 0);
    tap(&shell, USAGE_DOWN);
    CHECK(tap(&shell, USAGE_RIGHT) == PDA_SHELL_REDRAW);
    CHECK(strcmp(app_name(&shell), pda_apps[1]->name) == 0);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_REDRAW);
    CHECK(shell.app == PDA_SHELL_HOME);
    CHECK(tap(&shell, (uint8_t)(USAGE_1 + app_index("Calc"))) == PDA_SHELL_REDRAW);
    CHECK(strcmp(app_name(&shell), "Calc") == 0);
    tap(&shell, PDA_USAGE_ESCAPE);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_LEAVE);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_leaving_focus_keeps_the_view(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    open_named(&shell, "Keys");
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(strcmp(app_name(&shell), "Keys") == 0);
    // Focusing again resumes the same app.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(strcmp(app_name(&shell), "Keys") == 0);
}

static void test_quick_note_returns_to_the_view_it_replaced(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    open_named(&shell, "Timer");
    tap(&shell, USAGE_SPACE); // the timer runs
    CHECK(shell.tick_ms != 0);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);

    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    CHECK(strcmp(app_name(&shell), "Note") == 0);
    CHECK(shell.tick_ms == 0);
    type(&shell, "hi");
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_LEAVE);
    CHECK(strcmp(app_name(&shell), "Timer") == 0);
    CHECK(shell.tick_ms != 0); // the timer ticks again
    CHECK(strcmp(shell.note.text, "hi") == 0);

    // Cmd-Return types the note out and also returns.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    CHECK(tap_mods(&shell, LGUI, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(shell.type_out_len == 2 && memcmp(shell.type_out, "hi", 2) == 0);
    CHECK(shell.note.len == 0);
    CHECK(strcmp(app_name(&shell), "Timer") == 0);
}

static void test_slot_6_during_quick_note_keeps_note_open(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(shell.app == PDA_SHELL_HOME);
    CHECK(!shell.quick);
}

static void test_note_opened_from_home_stays_after_type_out(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    open_named(&shell, "Note");
    tap(&shell, USAGE_H);
    CHECK(tap_mods(&shell, LGUI, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(strcmp(app_name(&shell), "Note") == 0);
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) == PDA_SHELL_REDRAW);
    CHECK(shell.app == PDA_SHELL_HOME);
}

static void test_keys_app_tracks_positions(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_NONE); // not focused
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_NONE); // home has no positions
    open_named(&shell, "Keys");
    CHECK(pda_shell_position(&shell, 30, true) == PDA_SHELL_REDRAW);
    CHECK(shell.keys.held[30 / 8] & (1 << (30 % 8)));
    CHECK(shell.keys.last_position == 30);
    pda_shell_position(&shell, 30, false);
    CHECK(!(shell.keys.held[30 / 8] & (1 << (30 % 8))));
    CHECK(shell.keys.seen[30 / 8] & (1 << (30 % 8)));
    CHECK(pda_shell_position(&shell, 200, true) == PDA_SHELL_NONE);
    // Cmd-Backspace starts over.
    tap_mods(&shell, LGUI, PDA_USAGE_BACKSPACE);
    CHECK(!(shell.keys.seen[30 / 8] & (1 << (30 % 8))));
}

static void test_home_scrolls_to_the_highlight(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    static struct pda_fb fb;
    const int row_h = pda_text_line_height(PDA_FONT_BODY) + 3;
    const int first_row_y = PDA_TITLE_H + 2 + row_h / 2;

    shell.selected = 1;
    pda_shell_draw(&shell, &fb);
    CHECK(pda_fb_get(&fb, PDA_FB_W / 2, first_row_y + row_h));
    CHECK(!pda_fb_get(&fb, PDA_FB_W / 2, first_row_y));
    // The rule under the title.
    CHECK(pda_fb_get(&fb, 0, PDA_TITLE_H - 2));

    // The last app, past the bottom of the list, scrolls into the last visible row.
    shell.selected = pda_app_count - 1;
    pda_shell_draw(&shell, &fb);
    const int visible = (PDA_FB_H - PDA_TITLE_H - 2) / row_h;
    CHECK(visible < pda_app_count);
    CHECK(pda_fb_get(&fb, PDA_FB_W / 2, first_row_y + (visible - 1) * row_h));
}

static void test_clock_runs_from_the_report(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 1);
    struct pda_date_time t;
    CHECK(!pda_shell_time(&shell, &t));
    // 2026-10-12 21:05:30, reported 1 s before the uptime counter wraps.
    shell.clock = (struct pda_clock_state){PDA_CLOCK_SET, 1, 845154330u, 0xFFFFFC18u};
    shell.now_ms = 0xFFFFFC18u + 30000u;
    CHECK(pda_shell_time(&shell, &t));
    CHECK(t.year == 2026 && t.month == 10 && t.day == 12);
    CHECK(t.hour == 21 && t.minute == 6 && t.second == 0);
    shell.clock.status = PDA_CLOCK_NO_CTS;
    CHECK(!pda_shell_time(&shell, &t));
}

static void test_random_stays_below_n(void) {
    struct pda_shell shell;
    pda_shell_init(&shell, 0); // 0 is no seed; the shell picks one
    for (int i = 0; i < 1000; i++) {
        CHECK(pda_shell_random(&shell, 7) < 7);
    }
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
    test_zero_opens_the_tenth_app();
    test_leaving_focus_keeps_the_view();
    test_quick_note_returns_to_the_view_it_replaced();
    test_slot_6_during_quick_note_keeps_note_open();
    test_note_opened_from_home_stays_after_type_out();
    test_keys_app_tracks_positions();
    test_home_scrolls_to_the_highlight();
    test_clock_runs_from_the_report();
    test_random_stays_below_n();
    test_fb_clips();
    if (failures) {
        fprintf(stderr, "pda_shell_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_shell_test: all checks passed\n");
    return 0;
}
