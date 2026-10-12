/*
 * Dice: type a roll such as 2d6+3 or d20, then Return. Space rolls a d20, and Return on an
 * empty line rolls the latest expression again.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

#define MAX_SIDES 1000
#define MAX_BONUS 999

enum {
    USAGE_D = 0x07,
    USAGE_SPACE = 0x2C,
};

/** Reads an unsigned number at *p, if there is one; advances past it. */
static bool read_number(const char **p, int *out) {
    if (**p < '0' || **p > '9') {
        return false;
    }
    int n = 0;
    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        if (n > 100000) {
            return false;
        }
        (*p)++;
    }
    *out = n;
    return true;
}

/**
 * Parses NdM, NdM+K or NdM-K, where N (1 to PDA_DICE_ROLLS) may be left out for 1.
 * @return Whether text is such an expression.
 */
bool pda_dice_parse(const char *text, int *count, int *sides, int *bonus) {
    const char *p = text;
    *count = 1;
    *bonus = 0;
    if (*p != 'd' && (!read_number(&p, count) || *p != 'd')) {
        return false;
    }
    p++;
    if (!read_number(&p, sides) || *count < 1 || *count > PDA_DICE_ROLLS || *sides < 2 ||
        *sides > MAX_SIDES) {
        return false;
    }
    if (*p == '+' || *p == '-') {
        const int sign = *p == '-' ? -1 : 1;
        p++;
        if (!read_number(&p, bonus) || *bonus > MAX_BONUS) {
            return false;
        }
        *bonus *= sign;
    }
    return *p == '\0';
}

static void roll(struct pda_shell *shell, const char *expression) {
    struct pda_dice_state *d = &shell->dice;
    int count, sides, bonus;
    d->rolled = true;
    if (!pda_dice_parse(expression, &count, &sides, &bonus)) {
        d->invalid = true;
        return;
    }
    if (d->roll_count > 0 && !d->invalid) {
        memmove(d->history[1], d->history[0], sizeof(d->history[0]) * (PDA_DICE_HISTORY - 1));
        memmove(&d->history_total[1], &d->history_total[0],
                sizeof(d->history_total[0]) * (PDA_DICE_HISTORY - 1));
        memcpy(d->history[0], d->last, sizeof(d->last));
        d->history_total[0] = d->total;
    }
    d->invalid = false;
    snprintf(d->last, sizeof(d->last), "%s", expression);
    d->total = bonus;
    d->bonus = (int16_t)bonus;
    d->roll_count = (uint8_t)count;
    for (int i = 0; i < count; i++) {
        d->rolls[i] = (uint16_t)(1 + pda_shell_random(shell, (uint32_t)sides));
        d->total += d->rolls[i];
    }
}

static enum pda_app_result dice_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    struct pda_dice_state *d = &shell->dice;
    if (!pressed || (usage >= PDA_USAGE_LEFT_CONTROL && usage <= PDA_USAGE_RIGHT_GUI)) {
        return PDA_APP_NONE;
    }
    switch (usage) {
    case PDA_USAGE_ESCAPE:
        return PDA_APP_CLOSE;
    case USAGE_SPACE:
        roll(shell, "d20");
        return PDA_APP_REDRAW;
    case PDA_USAGE_RETURN:
    case PDA_USAGE_KP_ENTER:
        if (d->entry_len > 0) {
            roll(shell, d->entry);
            d->entry_len = 0;
            d->entry[0] = '\0';
        } else if (d->last[0]) {
            roll(shell, d->last);
        }
        return PDA_APP_REDRAW;
    case PDA_USAGE_BACKSPACE:
        if (d->entry_len > 0) {
            d->entry[--d->entry_len] = '\0';
        }
        return PDA_APP_REDRAW;
    }
    const char c = pda_keys_char(usage, (shell->held_mods | implicit_mods) & PDA_MODS_SHIFT);
    if (((c >= '0' && c <= '9') || c == 'd' || c == '+' || c == '-') &&
        d->entry_len < PDA_DICE_ENTRY) {
        d->entry[d->entry_len++] = c;
        d->entry[d->entry_len] = '\0';
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static void dice_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_dice_state *d = &shell->dice;
    char text[48];
    const int small_h = pda_text_line_height(PDA_FONT_SMALL);

    snprintf(text, sizeof(text), "%s_", d->entry);
    pda_text_draw(fb, area.x + 2, area.y + 3, area.w - 4, text, PDA_FONT_BODY, true);
    pda_fb_fill(fb, (struct pda_rect){area.x, area.y + 20, area.w, 1}, true);

    if (!d->rolled) {
        static const char *const hints[] = {"2d6+3 ret", "spc d20", "ret again"};
        for (int i = 0; i < 3; i++) {
            pda_text_draw(fb, area.x + 2, area.y + 30 + i * (small_h + 4), area.w - 4, hints[i],
                          PDA_FONT_SMALL, true);
        }
        return;
    }
    if (d->invalid) {
        pda_text_center(fb, area, area.y + 34, "?", PDA_FONT_TITLE, true);
        pda_text_center(fb, area, area.y + 54, "try 2d6+1", PDA_FONT_BODY, true);
        return;
    }

    pda_text_center(fb, area, area.y + 24, d->last, PDA_FONT_BODY, true);
    const int value = d->total < 0 ? 0 : d->total > 9999 ? 9999 : d->total;
    pda_fb_seven_segment_number(fb, area, area.y + 42, value, true);

    // The single rolls, as many as fit on two lines.
    int len = 0;
    for (int i = 0; i < d->roll_count && len < (int)sizeof(text) - 8; i++) {
        len += snprintf(text + len, sizeof(text) - len, "%s%u", i ? " " : "", d->rolls[i]);
    }
    if (d->bonus) {
        snprintf(text + len, sizeof(text) - len, " %+d", d->bonus);
    }
    if (d->roll_count > 1 || d->bonus) {
        const size_t total = strlen(text);
        const size_t first = pda_text_line_len(text, total, PDA_FONT_SMALL, area.w - 4);
        char line[48];
        snprintf(line, sizeof(line), "%.*s", (int)first, text);
        pda_text_draw(fb, area.x + 2, area.y + 70, area.w - 4, line, PDA_FONT_SMALL, true);
        if (first < total) {
            pda_text_draw(fb, area.x + 2, area.y + 70 + small_h, area.w - 4, text + first,
                          PDA_FONT_SMALL, true);
        }
    }

    for (int i = 0; i < PDA_DICE_HISTORY && d->history[i][0]; i++) {
        snprintf(text, sizeof(text), "%s %d", d->history[i], d->history_total[i]);
        pda_text_draw(fb, area.x + 2, area.y + area.h - (PDA_DICE_HISTORY - i) * small_h - 1,
                      area.w - 4, text, PDA_FONT_SMALL, true);
    }
}

const struct pda_app pda_app_dice_def = {
    .name = "Dice",
    .key = dice_key,
    .draw = dice_draw,
};
