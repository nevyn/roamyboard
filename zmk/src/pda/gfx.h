/*
 * The PDA's 1-bit framebuffer and the drawing API that apps use. Shapes are pure C; text
 * goes through pda_text_*, which zmk/src/display/pda_text.c implements with LVGL's fonts and
 * zmk/tests stubs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <roamyboard/pda.h>

#define PDA_FB_W ROAMYBOARD_PDA_FB_WIDTH
#define PDA_FB_H ROAMYBOARD_PDA_FB_HEIGHT

/**
 * The whole display, upright as it is read: PDA_FB_W x PDA_FB_H pixels, one bit each, rows
 * top to bottom, ROAMYBOARD_PDA_FB_STRIDE bytes per row, MSB leftmost. A set bit is lit: the
 * PDA draws light on dark.
 */
struct pda_fb {
    uint8_t bits[ROAMYBOARD_PDA_FB_BYTES];
};

/** A rectangle; x and y are its top left corner. */
struct pda_rect {
    int x, y, w, h;
};

enum pda_font {
    /** Montserrat 12 with Latin-1, for running text. */
    PDA_FONT_BODY,
    /** Montserrat 14, ASCII, for titles. */
    PDA_FONT_TITLE,
    /** Unscii 8, monospaced ASCII, for dense data. */
    PDA_FONT_SMALL,
    /** Noto Sans JP 40, the basic hiragana and katakana only, for the Kana drill. */
    PDA_FONT_KANA,
};

void pda_fb_clear(struct pda_fb *fb);
bool pda_fb_get(const struct pda_fb *fb, int x, int y);
/** Sets (lit) or clears one pixel; pixels outside the display are ignored. */
void pda_fb_set(struct pda_fb *fb, int x, int y, bool lit);
/** Fills r, clipped to the display. */
void pda_fb_fill(struct pda_fb *fb, struct pda_rect r, bool lit);
/** Draws r's 1-pixel outline, clipped to the display. */
void pda_fb_frame(struct pda_fb *fb, struct pda_rect r, bool lit);
/** Inverts every pixel of r, clipped to the display. */
void pda_fb_invert(struct pda_fb *fb, struct pda_rect r);

/** Size of a seven-segment digit. */
#define PDA_SEGMENT_W 11
#define PDA_SEGMENT_H 22

/** Draws a seven-segment digit, 0 to 9, with its top left at (x, y). */
void pda_fb_seven_segment(struct pda_fb *fb, int x, int y, int digit, bool lit);

/**
 * Draws "ab:cd" in seven-segment digits, centred across area's width at y: two two-digit
 * numbers, such as hours and minutes; each is shown modulo 100.
 */
void pda_fb_seven_segment_pair(struct pda_fb *fb, struct pda_rect area, int y, int ab, int cd,
                               bool lit);

/** Draws value, 0 to 9999, in seven-segment digits centred across area's width at y. */
void pda_fb_seven_segment_number(struct pda_fb *fb, struct pda_rect area, int y, int value,
                                 bool lit);

/** Height of one line of font, in pixels. */
int pda_text_line_height(enum pda_font font);

/**
 * Returns how many bytes of text fill one line of at most max_w pixels: up to and including a
 * newline, else up to the last space that fits, else as many characters as fit; at least one
 * character, so that a caller always advances.
 */
size_t pda_text_line_len(const char *text, size_t len, enum pda_font font, int max_w);

/** Width of text in pixels on one line. */
int pda_text_width(const char *text, enum pda_font font);

/**
 * Draws one line of UTF-8 text with its top left at (x, y), cut at max_w pixels. lit selects
 * the pixels' value, so that text can be drawn dark on a lit band.
 */
void pda_text_draw(struct pda_fb *fb, int x, int y, int max_w, const char *text,
                   enum pda_font font, bool lit);
