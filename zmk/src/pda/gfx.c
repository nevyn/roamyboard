/*
 * SPDX-License-Identifier: MIT
 */

#include "gfx.h"

#include <string.h>

void pda_fb_clear(struct pda_fb *fb) { memset(fb->bits, 0, sizeof(fb->bits)); }

static bool inside(int x, int y) { return x >= 0 && y >= 0 && x < PDA_FB_W && y < PDA_FB_H; }

bool pda_fb_get(const struct pda_fb *fb, int x, int y) {
    if (!inside(x, y)) {
        return false;
    }
    return fb->bits[y * ROAMYBOARD_PDA_FB_STRIDE + x / 8] & (0x80 >> (x % 8));
}

void pda_fb_set(struct pda_fb *fb, int x, int y, bool lit) {
    if (!inside(x, y)) {
        return;
    }
    uint8_t *byte = &fb->bits[y * ROAMYBOARD_PDA_FB_STRIDE + x / 8];
    const uint8_t mask = (uint8_t)(0x80 >> (x % 8));
    *byte = lit ? (*byte | mask) : (*byte & ~mask);
}

static struct pda_rect clip(struct pda_rect r) {
    if (r.x < 0) {
        r.w += r.x;
        r.x = 0;
    }
    if (r.y < 0) {
        r.h += r.y;
        r.y = 0;
    }
    if (r.x + r.w > PDA_FB_W) {
        r.w = PDA_FB_W - r.x;
    }
    if (r.y + r.h > PDA_FB_H) {
        r.h = PDA_FB_H - r.y;
    }
    return r;
}

void pda_fb_fill(struct pda_fb *fb, struct pda_rect r, bool lit) {
    r = clip(r);
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            pda_fb_set(fb, x, y, lit);
        }
    }
}

void pda_fb_frame(struct pda_fb *fb, struct pda_rect r, bool lit) {
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    pda_fb_fill(fb, (struct pda_rect){r.x, r.y, r.w, 1}, lit);
    pda_fb_fill(fb, (struct pda_rect){r.x, r.y + r.h - 1, r.w, 1}, lit);
    pda_fb_fill(fb, (struct pda_rect){r.x, r.y, 1, r.h}, lit);
    pda_fb_fill(fb, (struct pda_rect){r.x + r.w - 1, r.y, 1, r.h}, lit);
}

#define SEG_T 2
#define SEG_GAP 2

/* Segments a to g as bits 0 to 6. */
static const uint8_t seven_segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                           0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void pda_fb_seven_segment(struct pda_fb *fb, int x, int y, int digit, bool lit) {
    const uint8_t s = seven_segments[digit];
    const int w = PDA_SEGMENT_W, h = PDA_SEGMENT_H, half = PDA_SEGMENT_H / 2;
    const struct pda_rect segments[7] = {
        {x + SEG_T, y, w - 2 * SEG_T, SEG_T},                    // a
        {x + w - SEG_T, y + SEG_T, SEG_T, half - SEG_T},         // b
        {x + w - SEG_T, y + half, SEG_T, half - SEG_T},          // c
        {x + SEG_T, y + h - SEG_T, w - 2 * SEG_T, SEG_T},        // d
        {x, y + half, SEG_T, half - SEG_T},                      // e
        {x, y + SEG_T, SEG_T, half - SEG_T},                     // f
        {x + SEG_T, y + half - SEG_T / 2, w - 2 * SEG_T, SEG_T}, // g
    };
    for (int i = 0; i < 7; i++) {
        if (s & (1 << i)) {
            pda_fb_fill(fb, segments[i], lit);
        }
    }
}

void pda_fb_seven_segment_pair(struct pda_fb *fb, struct pda_rect area, int y, int ab, int cd,
                               bool lit) {
    const int width = 4 * PDA_SEGMENT_W + 5 * SEG_GAP + SEG_T;
    int x = area.x + (area.w - width) / 2;
    pda_fb_seven_segment(fb, x, y, ab / 10 % 10, lit);
    x += PDA_SEGMENT_W + SEG_GAP;
    pda_fb_seven_segment(fb, x, y, ab % 10, lit);
    x += PDA_SEGMENT_W + SEG_GAP;
    pda_fb_fill(fb, (struct pda_rect){x, y + PDA_SEGMENT_H / 3 - 1, SEG_T, SEG_T}, lit);
    pda_fb_fill(fb, (struct pda_rect){x, y + 2 * PDA_SEGMENT_H / 3, SEG_T, SEG_T}, lit);
    x += SEG_T + SEG_GAP;
    pda_fb_seven_segment(fb, x, y, cd / 10 % 10, lit);
    x += PDA_SEGMENT_W + SEG_GAP;
    pda_fb_seven_segment(fb, x, y, cd % 10, lit);
}

void pda_fb_invert(struct pda_fb *fb, struct pda_rect r) {
    r = clip(r);
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            pda_fb_set(fb, x, y, !pda_fb_get(fb, x, y));
        }
    }
}
