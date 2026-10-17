/*
 * Words: guess the five-letter word in six tries. A filled cell is the right letter in the
 * right place, a framed cell a letter that the word has elsewhere. Any five letters count as a
 * guess, since the PDA has no dictionary. Under the grid, the letters not yet ruled out.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"
#include "words.h"

#define CELL_W 12
#define CELL_H 14
#define PITCH_X 13
#define PITCH_Y 16

static void start(struct pda_shell *shell) {
    struct pda_words_state *w = &shell->words;
    memset(w, 0, sizeof(*w));
    w->started = true;
    memcpy(w->answer,
           pda_words_answers[pda_shell_random(shell, (uint32_t)pda_words_answer_count)],
           PDA_WORDS_LEN + 1);
}

void pda_words_mark(const char *answer, const char *guess, uint8_t marks[PDA_WORDS_LEN]) {
    uint8_t left[26] = {0};
    for (int i = 0; i < PDA_WORDS_LEN; i++) {
        if (guess[i] == answer[i]) {
            marks[i] = PDA_WORDS_CORRECT;
        } else {
            marks[i] = PDA_WORDS_ABSENT;
            left[answer[i] - 'a']++;
        }
    }
    for (int i = 0; i < PDA_WORDS_LEN; i++) {
        if (marks[i] != PDA_WORDS_CORRECT && left[guess[i] - 'a'] > 0) {
            marks[i] = PDA_WORDS_PRESENT;
            left[guess[i] - 'a']--;
        }
    }
}

static void submit(struct pda_shell *shell) {
    struct pda_words_state *w = &shell->words;
    const int row = w->count++;
    memcpy(w->guesses[row], w->entry, PDA_WORDS_LEN + 1);
    pda_words_mark(w->answer, w->entry, w->marks[row]);
    bool all = true;
    for (int i = 0; i < PDA_WORDS_LEN; i++) {
        all = all && w->marks[row][i] == PDA_WORDS_CORRECT;
        if (!memchr(w->answer, w->entry[i], PDA_WORDS_LEN)) {
            w->absent |= 1u << (w->entry[i] - 'a');
        }
    }
    w->entry_len = 0;
    memset(w->entry, 0, sizeof(w->entry));
    if (all || w->count == PDA_WORDS_TRIES) {
        w->over = true;
        w->won = all;
        shell->scores.words_played++;
        shell->scores.words_won += all;
        shell->scores_changed = true;
    }
}

static void words_open(struct pda_shell *shell) {
    if (!shell->words.started) {
        start(shell);
    }
}

static enum pda_app_result words_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                     bool pressed) {
    (void)implicit_mods;
    struct pda_words_state *w = &shell->words;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    const bool enter = usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER;
    if (w->over) {
        if (enter || usage == 0x2C /* Space */) {
            start(shell);
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    if (enter && w->entry_len == PDA_WORDS_LEN) {
        submit(shell);
        return PDA_APP_REDRAW;
    }
    if (usage == PDA_USAGE_BACKSPACE && w->entry_len > 0) {
        w->entry[--w->entry_len] = '\0';
        return PDA_APP_REDRAW;
    }
    if (usage >= PDA_USAGE_A && usage < PDA_USAGE_A + 26 && w->entry_len < PDA_WORDS_LEN &&
        !(shell->held_mods & (PDA_MODS_GUI | PDA_MODS_CTRL | PDA_MODS_ALT))) {
        w->entry[w->entry_len++] = (char)('a' + usage - PDA_USAGE_A);
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static void draw_letter(struct pda_fb *fb, int x, int y, char letter, bool lit) {
    const char text[2] = {(char)(letter - 'a' + 'A'), '\0'};
    const int w = pda_text_width(text, PDA_FONT_BODY);
    pda_text_draw(fb, x + (CELL_W - w) / 2, y, CELL_W, text, PDA_FONT_BODY, lit);
}

static void words_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_words_state *w = &shell->words;
    const int x0 = area.x + (area.w - (PDA_WORDS_LEN * PITCH_X - 1)) / 2;
    const int y0 = area.y + 2;

    for (int row = 0; row < PDA_WORDS_TRIES; row++) {
        const int y = y0 + row * PITCH_Y;
        for (int i = 0; i < PDA_WORDS_LEN; i++) {
            const struct pda_rect c = {x0 + i * PITCH_X, y, CELL_W, CELL_H};
            if (row < w->count) {
                const uint8_t mark = w->marks[row][i];
                if (mark == PDA_WORDS_CORRECT) {
                    pda_fb_fill(fb, c, true);
                } else if (mark == PDA_WORDS_PRESENT) {
                    pda_fb_frame(fb, c, true);
                }
                draw_letter(fb, c.x, y + 1, w->guesses[row][i], mark != PDA_WORDS_CORRECT);
            } else if (row == w->count && !w->over) {
                pda_fb_fill(fb, (struct pda_rect){c.x, y + CELL_H - 1, CELL_W, 1}, true);
                if (i < w->entry_len) {
                    draw_letter(fb, c.x, y + 1, w->entry[i], true);
                }
            } else {
                pda_fb_set(fb, c.x + CELL_W / 2, y + CELL_H - 1, true);
            }
        }
    }

    // The letters still possible, eight to a line.
    const int small_h = pda_text_line_height(PDA_FONT_SMALL);
    const int letters_y = y0 + PDA_WORDS_TRIES * PITCH_Y + 2;
    char line[9];
    int n = 0, lines = 0;
    for (int c = 0; c < 26; c++) {
        if (w->absent & (1u << c)) {
            continue;
        }
        line[n++] = (char)('a' + c);
        if (n == 8 || c == 25) {
            line[n] = '\0';
            pda_text_draw(fb, area.x + 2, letters_y + lines++ * small_h, area.w - 2, line,
                          PDA_FONT_SMALL, true);
            n = 0;
        }
    }
    if (n > 0) {
        line[n] = '\0';
        pda_text_draw(fb, area.x + 2, letters_y + lines * small_h, area.w - 2, line,
                      PDA_FONT_SMALL, true);
    }

    if (w->over) {
        char first[24];
        char answer[PDA_WORDS_LEN + 1];
        for (int i = 0; i <= PDA_WORDS_LEN; i++) {
            answer[i] = (char)(w->answer[i] >= 'a' ? w->answer[i] - 'a' + 'A' : w->answer[i]);
        }
        if (w->won) {
            snprintf(first, sizeof(first), "in %u! %u/%u", w->count, shell->scores.words_won,
                     shell->scores.words_played);
        } else {
            snprintf(first, sizeof(first), "%s %u/%u", answer, shell->scores.words_won,
                     shell->scores.words_played);
        }
        const char *const banner[3] = {first, "spc: again", NULL};
        const int banner_h = 2 * pda_text_line_height(PDA_FONT_BODY) + 6;
        pda_game_banner(fb, area, area.y + area.h - banner_h - 1, banner);
    }
}

const struct pda_app pda_app_words_def = {
    .name = "Words",
    .open = words_open,
    .key = words_key,
    .draw = words_draw,
};
