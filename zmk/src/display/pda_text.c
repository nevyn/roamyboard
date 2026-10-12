/*
 * The PDA's text drawing (zmk/src/pda/gfx.h) with LVGL's fonts: each line is drawn on a hidden
 * scratch canvas and copied into the 1-bit framebuffer. Display work queue only.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <lvgl.h>
#include <misc/lv_text_private.h>

#include "../pda/gfx.h"

// Montserrat 12 with Latin-1, for notes in Swedish; docs/pda.md, Displays.
LV_FONT_DECLARE(roamyboard_font_pda_12);

#define SCRATCH_W ROAMYBOARD_PDA_FB_WIDTH
#define SCRATCH_H 20

static uint8_t scratch_buf[LV_CANVAS_BUF_SIZE(SCRATCH_W, SCRATCH_H, 8, LV_DRAW_BUF_STRIDE_ALIGN)];
static lv_obj_t *scratch;

static const lv_font_t *lv_font(enum pda_font font) {
    switch (font) {
    case PDA_FONT_TITLE:
        return &lv_font_montserrat_14;
    case PDA_FONT_SMALL:
        return &lv_font_unscii_8;
    case PDA_FONT_BODY:
        break;
    }
    return &roamyboard_font_pda_12;
}

int pda_text_line_height(enum pda_font font) { return lv_font_get_line_height(lv_font(font)); }

size_t pda_text_line_len(const char *text, size_t len, enum pda_font font, int max_w) {
    const uint32_t n = lv_text_get_next_line(text, len, lv_font(font), 0, max_w, NULL,
                                             LV_TEXT_FLAG_NONE);
    if (n > 0) {
        return n;
    }
    // Nothing fits: take one whole UTF-8 character anyway.
    size_t one = 1;
    while (one < len && ((uint8_t)text[one] & 0xC0) == 0x80) {
        one++;
    }
    return one;
}

int pda_text_width(const char *text, enum pda_font font) {
    return lv_text_get_width(text, strlen(text), lv_font(font), 0);
}

void pda_text_draw(struct pda_fb *fb, int x, int y, int max_w, const char *text,
                   enum pda_font font, bool lit) {
    if (scratch == NULL) {
        scratch = lv_canvas_create(lv_layer_top());
        lv_canvas_set_buffer(scratch, scratch_buf, SCRATCH_W, SCRATCH_H, LV_COLOR_FORMAT_L8);
        lv_obj_add_flag(scratch, LV_OBJ_FLAG_HIDDEN);
    }
    const lv_font_t *lv = lv_font(font);
    const int w = max_w < SCRATCH_W ? max_w : SCRATCH_W;
    const int h = lv_font_get_line_height(lv) < SCRATCH_H ? lv_font_get_line_height(lv) : SCRATCH_H;
    if (w <= 0) {
        return;
    }

    lv_canvas_fill_bg(scratch, lv_color_black(), LV_OPA_COVER);
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_white();
    dsc.font = lv;
    dsc.text = text;
    lv_layer_t layer;
    lv_canvas_init_layer(scratch, &layer);
    const lv_area_t coords = {0, 0, w - 1, h - 1};
    lv_draw_label(&layer, &dsc, &coords);
    lv_canvas_finish_layer(scratch, &layer);

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            if (lv_canvas_get_px(scratch, sx, sy).red >= 0x80) {
                pda_fb_set(fb, x + sx, y + sy, lit);
            }
        }
    }
}
