/*
 * Renders the status screen on the host with the firmware's own drawing code
 * (zmk/src/display/screen.c) and LVGL. Writes one PGM per scene, upright as the nice!view
 * is read: 68 x 160 px, 0 for ink and 255 for background. run.sh builds and runs it.
 *
 * Usage: mock OUTPUT_DIR
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl.h>
#include <roamyboard/chain_state.h>

#include "cat.h"
#include "screen.h"

#define WIDTH 160
#define HEIGHT 68
#define STRIDE (WIDTH / 8)
#define PALETTE_SIZE 8

static uint8_t draw_buf[PALETTE_SIZE + STRIDE * HEIGHT];
static uint8_t frame[HEIGHT][WIDTH];
static uint32_t fake_ms;

static uint32_t tick(void) { return fake_ms; }

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    const uint8_t *bits = px_map + PALETTE_SIZE;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            frame[y][x] = (bits[y * STRIDE + x / 8] >> (7 - x % 8)) & 1;
        }
    }
    lv_display_flush_ready(display);
}

/** Writes the frame upright: the display's right edge is the top, its top edge the left. */
static void write_scene(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.pgm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P5\n%d %d\n255\n", HEIGHT, WIDTH);
    for (int py = 0; py < WIDTH; py++) {
        for (int px = 0; px < HEIGHT; px++) {
            // Index 1 of the I1 display is white, as in the nice!view's LVGL setup.
            fputc(frame[px][WIDTH - 1 - py] ? 255 : 0, f);
        }
    }
    fclose(f);
}

static struct screen screen;

static void render(const char *dir, const char *name, const struct status_state *state,
                   enum cat_frame cat_frame, int cat_position) {
    screen_draw_top(&screen, state);
    screen_draw_middle(&screen, state);
    screen_draw_bottom(&screen, state);
    screen_show_cat(&screen, cat_frame, cat_position);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, name);
}

static struct status_state base_state(int key_module_count) {
    struct status_state state = {
        .battery = 80,
        .key_module_count = key_module_count,
#if STATUS_SCREEN_CENTRAL
        .selected_endpoint = {.transport = ZMK_TRANSPORT_BLE},
        .active_profile_index = 0,
        .active_profile_connected = true,
        .active_profile_bonded = true,
        .profiles_connected = {true},
        .profiles_bonded = {true, true},
        .layer_label = "QWERTY",
#else
        .connected = true,
#endif
    };
    return state;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT_DIR\n", argv[0]);
        return 2;
    }
    const char *dir = argv[1];

    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_FULL);

    lv_obj_t *root = lv_obj_create(NULL);
    screen_init(&screen, root);
    lv_screen_load(root);

#if STATUS_SCREEN_CENTRAL
    struct status_state state = base_state(7);
    render(dir, "central-7cols-walk", &state, CAT_WALK_1, 12);

    state = base_state(1);
    state.selected_endpoint.transport = ZMK_TRANSPORT_USB;
    state.charging = true;
    state.layer_label = "Keypad";
    render(dir, "central-1col-sit", &state, CAT_SIT, 20);

    state = base_state(0);
    state.active_profile_connected = false;
    state.profiles_connected[0] = false;
    render(dir, "central-0cols-sleep", &state, CAT_SLEEP_1, 30);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    render(dir, "central-noterm-blink", &state, CAT_BLINK, 0);

    state = base_state(ROAMYBOARD_CHAIN_UNKNOWN);
    render(dir, "central-unknown-walk", &state, CAT_WALK_0, CAT_TRACK_LENGTH - 1);

    screen_draw_bootloader(&screen);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, "central-bootloader");
#else
    struct status_state state = base_state(7);
    render(dir, "peripheral-7cols-walk", &state, CAT_WALK_2, 6);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    state.connected = false;
    render(dir, "peripheral-noterm-sit", &state, CAT_SIT, 36);

    screen_draw_bootloader(&screen);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, "peripheral-bootloader");
#endif
    return 0;
}
