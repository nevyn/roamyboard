/*
 * Calc: an RPN calculator with a four-level stack (X, Y, Z, T), as on HP's calculators.
 * Return or Space pushes X; an operator combines Y and X. Cmd-Return types X out to the host.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

#define X stack[0]
#define Y stack[1]

static void push(struct pda_calc_state *c) {
    c->stack[3] = c->stack[2];
    c->stack[2] = c->stack[1];
    c->stack[1] = c->stack[0];
}

static void drop(struct pda_calc_state *c) {
    c->stack[1] = c->stack[2];
    c->stack[2] = c->stack[3];
}

/** Ends typing: the entry becomes X. */
static void commit(struct pda_calc_state *c) {
    if (!c->entering) {
        return;
    }
    c->entry[c->entry_len] = '\0';
    c->X = strtod(c->entry, NULL);
    c->entering = false;
}

static void type_char(struct pda_calc_state *c, char ch) {
    if (!c->entering) {
        if (c->lift) {
            push(c);
        }
        c->entering = true;
        c->entry_len = 0;
    }
    if (ch == '.' && memchr(c->entry, '.', c->entry_len)) {
        return;
    }
    if (c->entry_len < sizeof(c->entry) - 1) {
        c->entry[c->entry_len++] = ch;
        c->entry[c->entry_len] = '\0';
    }
}

static bool binary(struct pda_calc_state *c, char op) {
    commit(c);
    double result;
    switch (op) {
    case '+':
        result = c->Y + c->X;
        break;
    case '-':
        result = c->Y - c->X;
        break;
    case '*':
        result = c->Y * c->X;
        break;
    case '/':
        if (c->X == 0) {
            return false;
        }
        result = c->Y / c->X;
        break;
    default:
        return false;
    }
    if (!isfinite(result)) {
        return false;
    }
    drop(c);
    c->X = result;
    c->lift = true;
    return true;
}

/**
 * Formats v in at most max_w pixels of font: plainly if it fits, then with fewer digits, then
 * in scientific notation.
 */
static void format(double v, char *buf, size_t size, int max_w, enum pda_font font) {
    static const char *const formats[] = {"%.10g", "%.8g", "%.6g", "%.3e", "%.1e"};
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        snprintf(buf, size, formats[i], v);
        if (pda_text_width(buf, font) <= max_w) {
            return;
        }
    }
}

static enum pda_app_result calc_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    struct pda_calc_state *c = &shell->calc;
    if (!pressed || (usage >= PDA_USAGE_LEFT_CONTROL && usage <= PDA_USAGE_RIGHT_GUI)) {
        return PDA_APP_NONE;
    }
    const uint8_t mods = shell->held_mods | implicit_mods;
    const bool enter = usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER;

    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    // An error shows until the next key, which then does its own work too.
    c->error = false;
    if ((mods & PDA_MODS_GUI) && enter) {
        commit(c);
        format(c->X, shell->type_out, sizeof(shell->type_out), 1000, PDA_FONT_BODY);
        shell->type_out_len = strlen(shell->type_out);
        c->lift = true;
        return PDA_APP_TYPE_OUT;
    }
    if (mods & (PDA_MODS_GUI | PDA_MODS_CTRL | PDA_MODS_ALT)) {
        return PDA_APP_NONE;
    }
    if (enter || usage == 0x2C /* Space */) {
        commit(c);
        push(c);
        c->lift = false;
        return PDA_APP_REDRAW;
    }
    if (usage == PDA_USAGE_BACKSPACE) {
        if (c->entering && c->entry_len > 1) {
            c->entry[--c->entry_len] = '\0';
        } else {
            c->entering = false;
            c->X = 0;
            c->lift = false;
        }
        return PDA_APP_REDRAW;
    }

    const char ch = pda_keys_char(usage, mods & PDA_MODS_SHIFT);
    if ((ch >= '0' && ch <= '9') || ch == '.') {
        type_char(c, ch);
        return PDA_APP_REDRAW;
    }
    switch (ch) {
    case '+':
    case '-':
    case '*':
    case '/':
        if (!binary(c, ch)) {
            c->error = true;
        }
        return PDA_APP_REDRAW;
    case 'n':
        if (c->entering && c->entry_len > 0 && c->entry[0] == '-') {
            memmove(c->entry, c->entry + 1, c->entry_len--);
        } else if (c->entering && c->entry_len < sizeof(c->entry) - 1) {
            memmove(c->entry + 1, c->entry, ++c->entry_len);
            c->entry[0] = '-';
        } else {
            c->X = -c->X;
            c->lift = true;
        }
        return PDA_APP_REDRAW;
    case 's': {
        commit(c);
        const double x = c->X;
        c->X = c->Y;
        c->Y = x;
        c->lift = true;
        return PDA_APP_REDRAW;
    }
    case 'r':
        commit(c);
        if (c->X < 0) {
            c->error = true;
        } else {
            c->X = sqrt(c->X);
            c->lift = true;
        }
        return PDA_APP_REDRAW;
    case 'c':
        memset(c, 0, sizeof(*c));
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static void draw_right(struct pda_fb *fb, struct pda_rect area, int y, const char *text,
                       enum pda_font font) {
    const int w = pda_text_width(text, font);
    const int x = area.x + area.w - 2 - w;
    pda_text_draw(fb, x < area.x + 1 ? area.x + 1 : x, y, area.w - 2, text, font, true);
}

static void calc_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_calc_state *c = &shell->calc;
    static const char *const labels[] = {"x", "y", "z", "t"};
    const int small_h = pda_text_line_height(PDA_FONT_SMALL);
    char text[24];

    for (int level = 3; level >= 1; level--) {
        const int y = area.y + 3 + (3 - level) * (small_h + 3);
        pda_text_draw(fb, area.x + 1, y, 10, labels[level], PDA_FONT_SMALL, true);
        format(c->stack[level], text, sizeof(text), area.w - 12, PDA_FONT_SMALL);
        draw_right(fb, area, y, text, PDA_FONT_SMALL);
    }
    const int rule_y = area.y + 3 + 3 * (small_h + 3);
    pda_fb_fill(fb, (struct pda_rect){area.x, rule_y, area.w, 1}, true);

    if (c->error) {
        snprintf(text, sizeof(text), "error");
    } else if (c->entering) {
        snprintf(text, sizeof(text), "%.*s_", c->entry_len, c->entry);
    } else {
        format(c->X, text, sizeof(text), area.w - 3, PDA_FONT_TITLE);
    }
    draw_right(fb, area, rule_y + 4, text, PDA_FONT_TITLE);

    static const char *const hints[] = {"ret push", "n neg", "s swap", "r sqrt", "c clear",
                                        "cmd-ret", " types x"};
    const int count = sizeof(hints) / sizeof(hints[0]);
    for (int i = 0; i < count; i++) {
        const int y = area.y + area.h - (count - i) * small_h - 1;
        pda_text_draw(fb, area.x + 1, y, area.w - 2, hints[i], PDA_FONT_SMALL, true);
    }
}

const struct pda_app pda_app_calc_def = {
    .name = "Calc",
    .key = calc_key,
    .draw = calc_draw,
};
