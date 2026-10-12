/*
 * Stub for zmk/src/display/pda_text.c in the host tests: every byte is 6 px wide (8 px in the
 * small font, which is monospaced), and drawing marks nothing.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "gfx.h"

int failures;

static int char_w(enum pda_font font) { return font == PDA_FONT_SMALL ? 8 : 6; }

int pda_text_line_height(enum pda_font font) { return font == PDA_FONT_SMALL ? 8 : 15; }

int pda_text_width(const char *text, enum pda_font font) {
    return char_w(font) * (int)strlen(text);
}

size_t pda_text_line_len(const char *text, size_t len, enum pda_font font, int max_w) {
    size_t n = 0;
    while (n < len && (int)(n + 1) * char_w(font) <= max_w) {
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
