/*
 * Renders the status screen on the host with the firmware's own drawing code
 * (zmk/src/display/screen.c) and LVGL. Writes two PGMs per scene, 0 for ink and 255 for
 * background: NAME-panel.pgm is the panel's own image, 160 x 68 px with line 1 at the top;
 * NAME-leg.pgm is what a person sees on the mounted nice!view, 68 x 160 px. Define
 * CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180 to flush like the firmware with that option.
 * run.sh builds and runs it.
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

/** Stores the flushed area in frame, as the panel receives it. */
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    lv_area_t panel_area = *area;
#ifdef CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180
    if (screen_rotate_180(&panel_area, px_map, WIDTH, HEIGHT)) {
        fprintf(stderr, "cannot rotate area (%d,%d)-(%d,%d)\n", (int)area->x1, (int)area->y1,
                (int)area->x2, (int)area->y2);
        exit(1);
    }
#endif
    const uint8_t *bits = px_map + PALETTE_SIZE;
    const int w = lv_area_get_width(&panel_area);
    for (int y = 0; y < lv_area_get_height(&panel_area); y++) {
        for (int x = 0; x < w; x++) {
            frame[panel_area.y1 + y][panel_area.x1 + x] =
                (bits[y * (w / 8) + x / 8] >> (7 - x % 8)) & 1;
        }
    }
    lv_display_flush_ready(display);
}

static FILE *open_pgm(const char *dir, const char *name, const char *kind, int w, int h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s-%s.pgm", dir, name, kind);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    return f;
}

// Index 1 of the I1 display is white, as in the nice!view's LVGL setup.
static int gray(uint8_t pixel) { return pixel ? 255 : 0; }

static void write_scene(const char *dir, const char *name) {
    FILE *f = open_pgm(dir, name, "panel", WIDTH, HEIGHT);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            fputc(gray(frame[y][x]), f);
        }
    }
    fclose(f);

    // On the leg, panel line 1 is on the reader's right and pixel 0 of each line at the top.
    f = open_pgm(dir, name, "leg", HEIGHT, WIDTH);
    for (int py = 0; py < WIDTH; py++) {
        for (int px = 0; px < HEIGHT; px++) {
            fputc(gray(frame[HEIGHT - 1 - px][py]), f);
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

/** Renders the first seconds after start: the count box shows the build ID, with no cat. */
static void render_startup(const char *dir, const char *name, struct status_state state,
                           const char *build_id) {
    state.startup_build_id = build_id;
    screen_draw_top(&screen, &state);
    screen_draw_middle(&screen, &state);
    screen_draw_bottom(&screen, &state);
    lv_obj_add_flag(screen.cat, LV_OBJ_FLAG_HIDDEN);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, name);
}

static void render_bootloader(const char *dir, const char *name,
                              enum roamyboard_bootloader_mode mode, const char *build_id) {
    screen_draw_bootloader(&screen, mode, build_id);
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
        // Profiles 1 and 4 connected, 2 paired, 3 and 5 open.
        .profiles_connected = {true, false, false, true, false},
        .profiles_bonded = {true, true, false, true, false},
        .layer_label = "QWERTY",
        .active_host = {.state = ROAMYBOARD_HOST_NAME_KNOWN, .name = "Alecto"},
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
    render_startup(dir, "central-startup", base_state(7), "3274ed3-dirty");
    render_startup(dir, "central-startup-unknown", base_state(7), "unknown");

    struct status_state state = base_state(7);
    render(dir, "central-7cols-walk", &state, CAT_WALK_1, 12);

    state = base_state(1);
    state.selected_endpoint.transport = ZMK_TRANSPORT_USB;
    state.charging = true;
    state.layer_label = "Keypad";
    state.active_profile_index = 3;
    snprintf(state.active_host.name, sizeof(state.active_host.name), "Nevyn's MacBook Pro");
    render(dir, "central-1col-sit", &state, CAT_SIT, 20);

    state = base_state(0);
    state.active_profile_index = 1;
    state.active_profile_connected = false;
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_PENDING};
    render(dir, "central-0cols-sleep", &state, CAT_SLEEP_1, 30);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_ATT_ERROR,
                                                      .error = 0x0e};
    render(dir, "central-noterm-blink", &state, CAT_BLINK, 0);

    state = base_state(ROAMYBOARD_CHAIN_UNKNOWN);
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_READ_ERROR,
                                                      .error = -12};
    render(dir, "central-unknown-walk", &state, CAT_WALK_0, CAT_TRACK_LENGTH - 1);

    state = base_state(7);
    state.active_profile_index = 2;
    state.active_profile_bonded = false;
    state.active_profile_connected = false;
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_NONE};
    render(dir, "central-open-profile", &state, CAT_SIT, 8);

    state = base_state(7);
    state.active_host.name[0] = '\0';
    render(dir, "central-empty-name", &state, CAT_WALK_2, 16);

    state = base_state(7);
    snprintf(state.active_host.name, sizeof(state.active_host.name),
             "Living Room iPad Pro (12.9-inch) (6th generation)");
    render(dir, "central-long-name", &state, CAT_WALK_3, 24);

    render_bootloader(dir, "central-bootloader", ROAMYBOARD_BOOTLOADER_UF2, "3274ed3-dirty");
    render_bootloader(dir, "central-ota", ROAMYBOARD_BOOTLOADER_OTA, "3274ed3");
#else
    render_startup(dir, "peripheral-startup", base_state(7), "3274ed3");

    struct status_state state = base_state(7);
    render(dir, "peripheral-7cols-walk", &state, CAT_WALK_2, 6);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    state.connected = false;
    render(dir, "peripheral-noterm-sit", &state, CAT_SIT, 36);

    render_bootloader(dir, "peripheral-bootloader", ROAMYBOARD_BOOTLOADER_UF2, "3274ed3");
    render_bootloader(dir, "peripheral-ota", ROAMYBOARD_BOOTLOADER_OTA, "3274ed3-dirty");
#endif
    return 0;
}
