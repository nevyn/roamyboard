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

void pda_fb_invert(struct pda_fb *fb, struct pda_rect r) {
    r = clip(r);
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            pda_fb_set(fb, x, y, !pda_fb_get(fb, x, y));
        }
    }
}
