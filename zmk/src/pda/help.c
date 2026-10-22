/*
 * SPDX-License-Identifier: MIT
 */

#include "help.h"

#include <string.h>

#define GAP 4
#define INDENT 3

/**
 * Lays out text wrapped to w from y: draws the lines that lie wholly inside area, inverted
 * as chips if keys, and returns the y under the last line.
 */
static int wrap(struct pda_fb *fb, struct pda_rect area, int x, int y, int w, const char *part,
                size_t len, bool keys) {
    static char text[160];
    static char line[96];
    len = len < sizeof(text) - 1 ? len : sizeof(text) - 1;
    memcpy(text, part, len); // the line breaker sees only this entry's keys or text
    text[len] = '\0';
    const int line_h = pda_text_line_height(PDA_FONT_BODY);
    for (size_t pos = 0; pos < len;) {
        const size_t n = pda_text_line_len(text + pos, len - pos, PDA_FONT_BODY, w);
        size_t shown = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
        memcpy(line, text + pos, shown);
        while (shown > 0 && (line[shown - 1] == ' ' || line[shown - 1] == '\n')) {
            shown--;
        }
        line[shown] = '\0';
        pos += n;
        if (fb != NULL && y >= area.y && y + line_h <= area.y + area.h) {
            pda_text_draw(fb, x, y, w, line, PDA_FONT_BODY, true);
            if (keys) {
                const int text_w = pda_text_width(line, PDA_FONT_BODY);
                pda_fb_invert(fb, (struct pda_rect){x - 2, y, text_w + 4, line_h});
            }
        }
        y += line_h;
    }
    return y;
}

int pda_help_draw(struct pda_fb *fb, struct pda_rect area, const char *help, int scroll) {
    static const char closing[] = "Esc\tcloses this help.";
    const int top = area.y - scroll;
    int y = top;
    for (int part = 0; part < 2; part++) {
        const char *text = part == 0 ? help : closing;
        while (*text) {
            const char *end = strchr(text, '\n');
            const size_t len = end ? (size_t)(end - text) : strlen(text);
            const char *tab = memchr(text, '\t', len);
            if (tab) {
                y = wrap(fb, area, area.x + 3, y, area.w - 6, text, (size_t)(tab - text), true);
                y = wrap(fb, area, area.x + INDENT, y, area.w - INDENT - 1, tab + 1,
                         len - (size_t)(tab - text) - 1, false);
            } else {
                y = wrap(fb, area, area.x + 1, y, area.w - 2, text, len, false);
            }
            y += GAP;
            text += len + (end ? 1 : 0);
        }
    }
    const int height = y - top;
    if (fb != NULL) {
        // Marks on the right edge where the help goes on above or below.
        if (scroll > 0) {
            pda_fb_fill(fb, (struct pda_rect){area.x + area.w - 2, area.y, 2, 6}, true);
        }
        if (height - scroll > area.h) {
            pda_fb_fill(fb, (struct pda_rect){area.x + area.w - 2, area.y + area.h - 6, 2, 6}, true);
        }
    }
    return height;
}
