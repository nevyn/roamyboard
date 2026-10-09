/*
 * Layout and drawing of the roamyboard status screen on the nice!view. Pure LVGL, so that
 * zmk/tools/screen_mock can render it on the host; status_screen.c feeds it ZMK state.
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

#include <roamyboard/status_screen.h>

#include "cat_frames.h"
#include "util.h"

#define STATUS_SCREEN_CENTRAL                                                                      \
    (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

/** The three canvases, top to bottom as the nice!view is read, upright. */
enum screen_canvas {
    SCREEN_TOP,
    SCREEN_MIDDLE,
    /** Only its top 24 rows are on the display. */
    SCREEN_BOTTOM,
    SCREEN_CANVAS_COUNT,
};

struct screen {
    lv_obj_t *obj;
    lv_obj_t *canvas[SCREEN_CANVAS_COUNT];
    lv_obj_t *cat;
    uint8_t cbuf[SCREEN_CANVAS_COUNT][CANVAS_BUF_SIZE];
};

/** Creates the screen's objects as children of parent, which should be 160 x 68 px. */
void screen_init(struct screen *screen, lv_obj_t *parent);

/**
 * Top canvas: output or split link, battery, and the key module count box, which shows
 * state->startup_build_id instead of the count when that is not NULL.
 */
void screen_draw_top(struct screen *screen, const struct status_state *state);

/** Middle canvas: Bluetooth profiles on the central and unibody, empty on the peripheral. */
void screen_draw_middle(struct screen *screen, const struct status_state *state);

/** Bottom canvas: the highest active layer's name on the central and unibody. */
void screen_draw_bottom(struct screen *screen, const struct status_state *state);

/**
 * Shows the cat in the key module count box.
 *
 * @param frame The frame to show.
 * @param position Distance from the left end of the box's floor, 0 to CAT_TRACK_LENGTH - 1.
 */
void screen_show_cat(struct screen *screen, enum cat_frame frame, int position);

/**
 * Replaces everything on the screen with the view for a bootloader mode, with build_id in
 * small text at the bottom.
 */
void screen_draw_bootloader(struct screen *screen, enum roamyboard_bootloader_mode mode,
                            const char *build_id);

/**
 * Turns one flushed area of a 1-bit display upside down, for a flush callback to pass on to
 * the panel's own.
 *
 * @param area In: the area that LVGL flushes. Out: where the rotated pixels go on the panel.
 * @param px_map The pixels as LVGL passes them to the flush callback for LV_COLOR_FORMAT_I1:
 *               an 8-byte palette, then rows of bits, MSB first. Rotated in place.
 * @param hor_res Width of the display in pixels.
 * @param ver_res Height of the display in pixels.
 * @return 0, or -EINVAL if the area's rows do not fill whole bytes; then nothing is changed.
 */
int screen_rotate_180(lv_area_t *area, uint8_t *px_map, int32_t hor_res, int32_t ver_res);
