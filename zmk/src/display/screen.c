/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <roamyboard/chain_state.h>

#include "cat.h"
#include "screen.h"

// The key module count box on the top canvas, in upright canvas coordinates.
#define BOX_Y 21
#define BOX_H 42
#define COUNT_Y (BOX_Y + 3)
#define CAT_X_MIN 3
#define CAT_Y (BOX_Y + BOX_H - 3 - CAT_HEIGHT)

_Static_assert(CAT_X_MIN + CAT_TRACK_LENGTH - 1 + CAT_WIDTH <= CANVAS_SIZE - 3,
               "the cat must stay inside the box");

/** Draws a build ID centered at y; a "-dirty" suffix wraps to a second line. */
static void draw_build_id(lv_obj_t *canvas, int y, const lv_font_t *font, const char *build_id) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, font, LV_TEXT_ALIGN_CENTER);
    canvas_draw_text(canvas, 1, y, CANVAS_SIZE - 3, &label_dsc, build_id);
}

static void draw_count(lv_obj_t *canvas, int key_module_count) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);

    char text[12];
    switch (key_module_count) {
    case ROAMYBOARD_CHAIN_UNKNOWN:
        strcpy(text, "...");
        break;
    case ROAMYBOARD_CHAIN_FAULT:
        // Montserrat 16 would fill the box from border to border.
        label_dsc.font = &lv_font_montserrat_14;
        strcpy(text, "no term");
        break;
    default:
        snprintf(text, sizeof(text), "%d col%s", key_module_count,
                 key_module_count == 1 ? "" : "s");
        break;
    }
    canvas_draw_text(canvas, 1, COUNT_Y, CANVAS_SIZE - 2, &label_dsc, text);
}

void screen_draw_top(struct screen *screen, const struct status_state *state) {
    lv_obj_t *canvas = screen->canvas[SCREEN_TOP];

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    // Fill background
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    // Draw battery
    draw_battery(canvas, state);

    // Draw output status
#if STATUS_SCREEN_CENTRAL
    char output_text[10] = {};

    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcat(output_text, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state->active_profile_bonded) {
            if (state->active_profile_connected) {
                strcat(output_text, LV_SYMBOL_WIFI);
            } else {
                strcat(output_text, LV_SYMBOL_CLOSE);
            }
        } else {
            strcat(output_text, LV_SYMBOL_SETTINGS);
        }
        break;
    case ZMK_TRANSPORT_NONE:
        break;
    }

    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc, output_text);
#else
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                     state->connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
#endif

    // Draw the key module count box; the cat is a separate image on top of it.
    canvas_draw_rect(canvas, 0, BOX_Y, CANVAS_SIZE, BOX_H, &rect_white_dsc);
    canvas_draw_rect(canvas, 1, BOX_Y + 1, CANVAS_SIZE - 2, BOX_H - 2, &rect_black_dsc);
    if (state->startup_build_id != NULL) {
        draw_build_id(canvas, COUNT_Y, &lv_font_montserrat_12, state->startup_build_id);
    } else {
        draw_count(canvas, state->key_module_count);
    }

    // Rotate canvas
    rotate_canvas(canvas);
}

#if STATUS_SCREEN_CENTRAL

// The profile markers along the top of the middle canvas, and the host name below them.
#define MARKER_Y 7
#define ACTIVE_MARKER_R 7
#define MARKER_R 3
#define NAME_Y 17
#define NAME_LINES 3

static const int marker_x[NICEVIEW_PROFILE_COUNT] = {7, 20, 34, 47, 60};

static void draw_profile_markers(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_arc_dsc_t ring_dsc;
    init_arc_dsc(&ring_dsc, LVGL_FOREGROUND, 1);
    lv_draw_arc_dsc_t disc_dsc;
    init_arc_dsc(&disc_dsc, LVGL_FOREGROUND, MARKER_R);
    lv_draw_arc_dsc_t active_dsc;
    init_arc_dsc(&active_dsc, LVGL_FOREGROUND, ACTIVE_MARKER_R);
    lv_draw_label_dsc_t number_dsc;
    init_label_dsc(&number_dsc, LVGL_BACKGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; i++) {
        const int x = marker_x[i];
        if (i == state->active_profile_index) {
            canvas_draw_arc(canvas, x, MARKER_Y, ACTIVE_MARKER_R, 0, 359, &active_dsc);
            char number[2];
            snprintf(number, sizeof(number), "%d", i + 1);
            canvas_draw_text(canvas, x - 4, MARKER_Y - 3, 8, &number_dsc, number);
        } else if (state->profiles_connected[i]) {
            canvas_draw_arc(canvas, x, MARKER_Y, MARKER_R, 0, 359, &disc_dsc);
        } else if (state->profiles_bonded[i]) {
            canvas_draw_arc(canvas, x, MARKER_Y, MARKER_R, 0, 359, &ring_dsc);
        } else {
            canvas_draw_arc(canvas, x, MARKER_Y, 1, 0, 359, &ring_dsc);
        }
    }
}

static void host_text(const struct status_state *state, char *text, size_t size) {
    const struct roamyboard_host_name *host = &state->active_host;
    switch (host->state) {
    case ROAMYBOARD_HOST_NAME_NONE:
        snprintf(text, size, "%s", state->active_profile_bonded ? "no name" : "open");
        break;
    case ROAMYBOARD_HOST_NAME_PENDING:
        snprintf(text, size, "...");
        break;
    case ROAMYBOARD_HOST_NAME_KNOWN:
        snprintf(text, size, "%s", host->name[0] ? host->name : "(empty)");
        break;
    case ROAMYBOARD_HOST_NAME_READ_ERROR:
        // Line breaks keep LVGL from wrapping at the minus sign.
        snprintf(text, size, "read err\n%d", host->error);
        break;
    case ROAMYBOARD_HOST_NAME_ATT_ERROR:
        snprintf(text, size, "att err\n0x%02x", (unsigned int)host->error);
        break;
    }
}

static bool text_fits(const char *text, const lv_font_t *font) {
    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, 0, CANVAS_SIZE, LV_TEXT_FLAG_NONE);
    return size.y <= NAME_LINES * lv_font_get_line_height(font);
}

/**
 * Returns the largest font that fits text in NAME_LINES lines, and shortens text to end in
 * ".." if even the smallest does not.
 */
static const lv_font_t *fit_text(char *text, size_t size) {
    static const lv_font_t *const fonts[] = {&lv_font_montserrat_14, &lv_font_montserrat_12};
    const size_t font_count = sizeof(fonts) / sizeof(fonts[0]);
    for (size_t i = 0; i < font_count; i++) {
        if (text_fits(text, fonts[i])) {
            return fonts[i];
        }
    }

    const lv_font_t *font = fonts[font_count - 1];
    size_t len = strlen(text);
    while (len > 0) {
        // Drop the last character, with any UTF-8 continuation bytes, and the spaces before it.
        do {
            len--;
        } while (len > 0 && ((uint8_t)text[len] & 0xC0) == 0x80);
        while (len > 0 && text[len - 1] == ' ') {
            len--;
        }
        if (len + 3 > size) {
            continue;
        }
        memcpy(text + len, "..", 3);
        if (text_fits(text, font)) {
            break;
        }
    }
    return font;
}

#endif // STATUS_SCREEN_CENTRAL

void screen_draw_middle(struct screen *screen, const struct status_state *state) {
    lv_obj_t *canvas = screen->canvas[SCREEN_MIDDLE];

    // Fill background
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

#if STATUS_SCREEN_CENTRAL
    draw_profile_markers(canvas, state);

    char text[ROAMYBOARD_HOST_NAME_MAX + 3];
    host_text(state, text, sizeof(text));
    lv_draw_label_dsc_t name_dsc;
    init_label_dsc(&name_dsc, LVGL_FOREGROUND, fit_text(text, sizeof(text)),
                   LV_TEXT_ALIGN_CENTER);
    canvas_draw_text(canvas, 0, NAME_Y, CANVAS_SIZE - 1, &name_dsc, text);
#else
    (void)state;
#endif

    // Rotate canvas
    rotate_canvas(canvas);
}

void screen_draw_bottom(struct screen *screen, const struct status_state *state) {
    lv_obj_t *canvas = screen->canvas[SCREEN_BOTTOM];

    // Fill background
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

#if STATUS_SCREEN_CENTRAL
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    // Draw layer
    if (state->layer_label == NULL || strlen(state->layer_label) == 0) {
        char text[10] = {};

        sprintf(text, "LAYER %i", state->layer_index);

        canvas_draw_text(canvas, 0, 5, 68, &label_dsc, text);
    } else {
        canvas_draw_text(canvas, 0, 5, 68, &label_dsc, state->layer_label);
    }
#else
    (void)state;
#endif

    // Rotate canvas
    rotate_canvas(canvas);
}

void screen_show_cat(struct screen *screen, enum cat_frame frame, int position) {
    lv_image_set_src(screen->cat, cat_frames[frame]);
    // Upright (x, y) on the top canvas lands at (CANVAS_SIZE - 1 - y, x) once rotated.
    lv_obj_align(screen->cat, LV_ALIGN_TOP_RIGHT, -CAT_Y, CAT_X_MIN + position);
    lv_obj_remove_flag(screen->cat, LV_OBJ_FLAG_HIDDEN);
}

static void draw_download_icon(lv_obj_t *canvas, int y) {
    lv_draw_rect_dsc_t ink;
    init_rect_dsc(&ink, LVGL_FOREGROUND);

    const int cx = CANVAS_SIZE / 2;
    // Arrow: shaft, then a head that narrows by one pixel per row on each side.
    canvas_draw_rect(canvas, cx - 3, y, 6, 9, &ink);
    for (int row = 0; row < 8; row++) {
        canvas_draw_rect(canvas, cx - 8 + row, y + 9 + row, 16 - 2 * row, 1, &ink);
    }
    // Tray under the arrow.
    const int tray_y = y + 14;
    canvas_draw_rect(canvas, cx - 16, tray_y, 3, 8, &ink);
    canvas_draw_rect(canvas, cx + 13, tray_y, 3, 8, &ink);
    canvas_draw_rect(canvas, cx - 16, tray_y + 7, 32, 3, &ink);
}

void screen_draw_bootloader(struct screen *screen, enum roamyboard_bootloader_mode mode,
                            const char *build_id) {
    lv_obj_add_flag(screen->cat, LV_OBJ_FLAG_HIDDEN);

    lv_draw_label_dsc_t title_dsc;
    init_label_dsc(&title_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t text_dsc;
    init_label_dsc(&text_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t small_dsc;
    init_label_dsc(&small_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *top = screen->canvas[SCREEN_TOP];
    lv_obj_t *middle = screen->canvas[SCREEN_MIDDLE];
    lv_canvas_fill_bg(top, LVGL_BACKGROUND, LV_OPA_COVER);
    lv_canvas_fill_bg(middle, LVGL_BACKGROUND, LV_OPA_COVER);

    switch (mode) {
    case ROAMYBOARD_BOOTLOADER_UF2:
        canvas_draw_text(top, 0, 4, CANVAS_SIZE, &title_dsc, "BOOT");
        // The widest line that fits the 68 px canvas in Montserrat 14; 16 wraps.
        canvas_draw_text(top, 0, 24, CANVAS_SIZE, &text_dsc, "LOADER");
        draw_download_icon(top, 44);

        canvas_draw_text(middle, 0, 8, CANVAS_SIZE, &text_dsc, "drop a");
        canvas_draw_text(middle, 0, 26, CANVAS_SIZE, &text_dsc, "UF2 on");
        canvas_draw_text(middle, 0, 48, CANVAS_SIZE, &small_dsc, "NICENANO");
        break;

    case ROAMYBOARD_BOOTLOADER_OTA: {
        canvas_draw_text(top, 0, 4, CANVAS_SIZE, &title_dsc, "OTA");
        canvas_draw_text(top, 0, 24, CANVAS_SIZE, &text_dsc, "BOOT");
        lv_draw_label_dsc_t glyph_dsc;
        init_label_dsc(&glyph_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18,
                       LV_TEXT_ALIGN_CENTER);
        canvas_draw_text(top, 0, 44, CANVAS_SIZE, &glyph_dsc, LV_SYMBOL_BLUETOOTH);

        // "Bluetooth" is too wide for the canvas in Montserrat 14.
        lv_draw_label_dsc_t hint_dsc;
        init_label_dsc(&hint_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12,
                       LV_TEXT_ALIGN_CENTER);
        canvas_draw_text(middle, 0, 6, CANVAS_SIZE - 1, &hint_dsc,
                         "send the DFU zip over Bluetooth");
        break;
    }
    }
    rotate_canvas(top);
    rotate_canvas(middle);

    lv_obj_t *bottom = screen->canvas[SCREEN_BOTTOM];
    lv_canvas_fill_bg(bottom, LVGL_BACKGROUND, LV_OPA_COVER);
    // Two lines of 8 px fit the 24 visible rows of the bottom canvas.
    draw_build_id(bottom, 3, &lv_font_unscii_8, build_id);
    rotate_canvas(bottom);
}

static uint8_t reverse_bits(uint8_t b) {
    b = (uint8_t)((b & 0xF0) >> 4 | (b & 0x0F) << 4);
    b = (uint8_t)((b & 0xCC) >> 2 | (b & 0x33) << 2);
    return (uint8_t)((b & 0xAA) >> 1 | (b & 0x55) << 1);
}

int screen_rotate_180(lv_area_t *area, uint8_t *px_map, int32_t hor_res, int32_t ver_res) {
    const int32_t width = lv_area_get_width(area);
    const uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1);
    if (width % 8 != 0 || stride * 8 != (uint32_t)width) {
        return -EINVAL;
    }

    // Without padding bits, the area is one bit string, and turning it upside down reverses it.
    uint8_t *bits = px_map + LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * 4;
    const size_t len = stride * lv_area_get_height(area);
    for (size_t i = 0, j = len - 1; i < j; i++, j--) {
        const uint8_t b = bits[i];
        bits[i] = reverse_bits(bits[j]);
        bits[j] = reverse_bits(b);
    }
    if (len % 2) {
        bits[len / 2] = reverse_bits(bits[len / 2]);
    }

    const lv_area_t flushed = *area;
    area->x1 = hor_res - 1 - flushed.x2;
    area->x2 = hor_res - 1 - flushed.x1;
    area->y1 = ver_res - 1 - flushed.y2;
    area->y2 = ver_res - 1 - flushed.y1;
    return 0;
}

void screen_init(struct screen *screen, lv_obj_t *parent) {
    screen->obj = lv_obj_create(parent);
    lv_obj_set_size(screen->obj, 160, 68);
    lv_obj_t *top = lv_canvas_create(screen->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, screen->cbuf[SCREEN_TOP], CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);
    lv_obj_t *middle = lv_canvas_create(screen->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, screen->cbuf[SCREEN_MIDDLE], CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);
    lv_obj_t *bottom = lv_canvas_create(screen->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, screen->cbuf[SCREEN_BOTTOM], CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);

    screen->canvas[SCREEN_TOP] = top;
    screen->canvas[SCREEN_MIDDLE] = middle;
    screen->canvas[SCREEN_BOTTOM] = bottom;

    // Created last, so that it is drawn over the top canvas.
    screen->cat = lv_image_create(screen->obj);
    lv_obj_add_flag(screen->cat, LV_OBJ_FLAG_HIDDEN);
}
