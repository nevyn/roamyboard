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
#include <roamyboard/pda.h>

#include "cat.h"
#include "screen.h"
#include "shell.h"

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

static void render(const char *dir, const char *name, const struct status_state *state) {
    screen_draw_top(&screen, state);
    screen_draw_middle(&screen, state);
    screen_draw_bottom(&screen, state);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, name);
}

#if STATUS_SCREEN_CENTRAL
static void render_cat(const char *dir, const char *name, const struct status_state *state,
                       enum cat_frame frame, int x, int y) {
    screen_show_cat(&screen, frame, x, y);
    render(dir, name, state);
}
#endif

/** Renders the first seconds after start: the count box shows the build ID, with no cat. */
static void render_startup(const char *dir, const char *name, struct status_state state,
                           const char *build_id) {
    state.startup_build_id = build_id;
    screen_draw_top(&screen, &state);
    screen_draw_middle(&screen, &state);
    screen_draw_bottom(&screen, &state);
#if STATUS_SCREEN_CENTRAL
    lv_obj_add_flag(screen.cat_box, LV_OBJ_FLAG_HIDDEN);
#endif
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

#if !STATUS_SCREEN_CENTRAL
static void render_shell(const char *dir, const char *name, const struct pda_shell *shell) {
    static struct pda_fb fb;
    pda_shell_draw(shell, &fb);
    screen_draw_pda(&screen, fb.bits, shell->mode != ROAMYBOARD_PDA_OFF);
    fake_ms += 1000;
    lv_refr_now(NULL);
    write_scene(dir, name);
}

static void tap(struct pda_shell *shell, uint8_t usage) {
    pda_shell_key(shell, usage, 0, true);
    pda_shell_key(shell, usage, 0, false);
}

static void open_named(struct pda_shell *shell, const char *name) {
    pda_shell_set_mode(shell, ROAMYBOARD_PDA_HOME);
    while (shell->app != PDA_SHELL_HOME) {
        tap(shell, 0x29);
    }
    for (int i = 0; i < pda_app_count; i++) {
        if (strcmp(pda_apps[i]->name, name) == 0) {
            while (shell->selected != i) {
                tap(shell, 0x4F); // Right
            }
            tap(shell, 0x28); // Return
        }
    }
}

static void type_letters(struct pda_shell *shell, const char *letters) {
    for (const char *c = letters; *c; c++) {
        tap(shell, (uint8_t)(0x04 + (*c - 'a')));
    }
}

/** Advances the mock's time by ms in the open app's ticks. */
static void run_ms(struct pda_shell *shell, uint32_t ms) {
    const uint32_t end = shell->now_ms + ms;
    while ((int32_t)(end - shell->now_ms) > 0) {
        shell->now_ms += pda_shell_tick_ms(shell) ? pda_shell_tick_ms(shell) : ms;
        pda_shell_tick(shell);
    }
}

static void render_pda_scenes(const char *dir) {
    static struct pda_shell shell;
    pda_shell_init(&shell, 7);
    shell.now_ms = 1000;
    shell.right = (struct pda_half_info){true, 7, 80, "9e2e912"};
    shell.left = (struct pda_half_info){true, 7, 64, "9e2e912-dirty"};
    render_shell(dir, "peripheral-pda-home-no-clock", &shell);
    shell.clock = (struct pda_clock_state){PDA_CLOCK_SET, 1, 845154330u, 1000}; // 21:05
    render_shell(dir, "peripheral-pda-home", &shell);

    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    tap(&shell, 0x51); // Down
    render_shell(dir, "peripheral-pda-home-focused", &shell);
    for (int i = 0; i < pda_app_count - 2; i++) {
        tap(&shell, 0x51);
    }
    render_shell(dir, "peripheral-pda-home-scrolled", &shell);

    open_named(&shell, "Keys");
    for (int position = 5; position < 120; position += 13) {
        pda_shell_position(&shell, (uint8_t)position, true);
        pda_shell_position(&shell, (uint8_t)position, false);
    }
    pda_shell_position(&shell, 37, true);
    pda_shell_key(&shell, 0x0A, 0, true);
    render_shell(dir, "peripheral-pda-keys", &shell);
    pda_shell_position(&shell, 37, false);
    pda_shell_key(&shell, 0x0A, 0, false);

    open_named(&shell, "Info");
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    render_shell(dir, "peripheral-pda-info", &shell);

    open_named(&shell, "Clock");
    render_shell(dir, "peripheral-pda-clock", &shell);
    const struct pda_clock_state clock = shell.clock;
    shell.clock.status = PDA_CLOCK_NO_CTS;
    render_shell(dir, "peripheral-pda-clock-unset", &shell);
    shell.clock = clock;

    open_named(&shell, "Timer");
    tap(&shell, 0x13); // P: 25 minutes
    tap(&shell, 0x2C);
    run_ms(&shell, 12 * 60000 + 26000);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    render_shell(dir, "peripheral-pda-timer", &shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    run_ms(&shell, 13 * 60000);
    render_shell(dir, "peripheral-pda-timer-done", &shell);

    open_named(&shell, "Calc");
    const uint8_t calc_keys[] = {0x1F, 0x28, 0x20, 0x28, 0x21, 0x22, 0x37, 0x24};
    for (size_t i = 0; i < sizeof(calc_keys); i++) {
        tap(&shell, calc_keys[i]); // 2 Enter 3 Enter 45.7
    }
    render_shell(dir, "peripheral-pda-calc", &shell);

    open_named(&shell, "Dice");
    const uint8_t dice_keys[] = {0x1F, 0x07, 0x23, 0x28, 0x2C, 0x21, 0x07, 0x23};
    for (size_t i = 0; i < sizeof(dice_keys); i++) {
        tap(&shell, dice_keys[i]); // 2d6 Return, Space (d20), 4d6
    }
    pda_shell_key(&shell, 0xE1, 0, true);
    tap(&shell, 0x2E); // +
    pda_shell_key(&shell, 0xE1, 0, false);
    tap(&shell, 0x1E); // 1
    tap(&shell, 0x28);
    render_shell(dir, "peripheral-pda-dice", &shell);

    open_named(&shell, "Kana");
    shell.kana.current = 11; // し
    render_shell(dir, "peripheral-pda-kana", &shell);
    type_letters(&shell, "sh");
    render_shell(dir, "peripheral-pda-kana-typing", &shell);
    type_letters(&shell, "i");
    render_shell(dir, "peripheral-pda-kana-right", &shell);
    tap(&shell, 0x28);
    shell.kana.current = 17; // つ
    type_letters(&shell, "ts");
    tap(&shell, 0x2B); // Tab: katakana
    shell.kana.current = 46 + 27; // フ
    type_letters(&shell, "ho");
    render_shell(dir, "peripheral-pda-kana-wrong", &shell);

    open_named(&shell, "Cat");
    run_ms(&shell, 1800);
    render_shell(dir, "peripheral-pda-cat", &shell);
    tap(&shell, 0x28); // Return: pet
    run_ms(&shell, 300);
    tap(&shell, 0x28);
    run_ms(&shell, 400);
    render_shell(dir, "peripheral-pda-cat-pet", &shell);
    shell.cat.needs.fun = 20;
    tap(&shell, 0x2C); // Space: the ball
    run_ms(&shell, 2600);
    render_shell(dir, "peripheral-pda-cat-ball", &shell);
    tap(&shell, 0x4F); // Right: the laser dot
    tap(&shell, 0x52);
    tap(&shell, 0x52);
    run_ms(&shell, 2500);
    render_shell(dir, "peripheral-pda-cat-laser", &shell);
    shell.cat.ball = false;
    shell.cat.laser = false;
    shell.cat.needs.food = 25;
    tap(&shell, 0x09); // F: fill the bowl
    run_ms(&shell, 6000);
    render_shell(dir, "peripheral-pda-cat-eat", &shell);
    tap(&shell, 0x29); // Escape twice: home at once
    tap(&shell, 0x29);
    shell.cat.needs = (struct pda_cat_saved){.food = 90, .fun = 70, .energy = 15, .bowl = 2};
    open_named(&shell, "Cat");
    run_ms(&shell, 25000);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    render_shell(dir, "peripheral-pda-cat-sleep", &shell);

    open_named(&shell, "Bricks");
    render_shell(dir, "peripheral-pda-bricks-start", &shell);
    tap(&shell, 0x28);
    tap(&shell, 0x2C);
    run_ms(&shell, 3500);
    render_shell(dir, "peripheral-pda-bricks", &shell);

    open_named(&shell, "Tetris");
    render_shell(dir, "peripheral-pda-tetris-start", &shell);
    tap(&shell, 0x28);
    const uint8_t moves[] = {0x50, 0x50, 0x50, 0x2C, 0x4F, 0x4F, 0x2C, 0x52, 0x2C,
                             0x50, 0x2C, 0x4F, 0x4F, 0x4F, 0x4F, 0x2C, 0x52, 0x50};
    for (size_t i = 0; i < sizeof(moves); i++) {
        tap(&shell, moves[i]);
    }
    run_ms(&shell, 1500);
    render_shell(dir, "peripheral-pda-tetris", &shell);

    open_named(&shell, "Snake");
    tap(&shell, 0x28);
    for (int i = 0; i < 6; i++) {
        shell.snake.food = (uint16_t)(shell.snake.body[shell.snake.head] + 1);
        run_ms(&shell, 160);
    }
    tap(&shell, 0x52); // Up
    run_ms(&shell, 1200);
    tap(&shell, 0x50); // Left
    run_ms(&shell, 600);
    render_shell(dir, "peripheral-pda-snake", &shell);

    open_named(&shell, "Words");
    memcpy(shell.words.answer, "crane", 6);
    type_letters(&shell, "slate");
    tap(&shell, 0x28);
    type_letters(&shell, "pride");
    tap(&shell, 0x28);
    type_letters(&shell, "cra");
    render_shell(dir, "peripheral-pda-words", &shell);
    type_letters(&shell, "ne");
    tap(&shell, 0x28);
    render_shell(dir, "peripheral-pda-words-won", &shell);

    open_named(&shell, "Rain");
    tap(&shell, 0x28);
    run_ms(&shell, 7000);
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        if (shell.rain.drops[i].active) {
            tap(&shell, (uint8_t)(0x04 + shell.rain.drops[i].word[0] - 'a'));
            tap(&shell, (uint8_t)(0x04 + shell.rain.drops[i].word[1] - 'a'));
            break;
        }
    }
    render_shell(dir, "peripheral-pda-rain", &shell);

    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    const char *text = "Köp mjölk\nÅÄÖ åäö\nRing Theo";
    pda_note_set_text(&shell.note, text, strlen(text));
    render_shell(dir, "peripheral-pda-note", &shell);
    pda_note_set_text(&shell.note, "", 0);

    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    struct pda_midi_view midi = {.shown = true, .velocity_step = 3, .last_note = 64};
    static const int chord[] = {48, 60, 64, 67, 70};
    for (size_t i = 0; i < sizeof(chord) / sizeof(chord[0]); i++) {
        midi.held[chord[i] / 8] |= 1 << (chord[i] % 8);
    }
    pda_shell_midi(&shell, &midi);
    render_shell(dir, "peripheral-midi-view", &shell);
    midi.shown = false;
    pda_shell_midi(&shell, &midi);
}
#endif

static struct status_state base_state(int key_module_count) {
    struct status_state state = {
        .battery = 80,
        .key_module_count = key_module_count,
        .show_count = true,
#if STATUS_SCREEN_CENTRAL
        .selected_endpoint = {.transport = ZMK_TRANSPORT_BLE},
        .active_profile_index = 0,
        .active_profile_connected = true,
        .active_profile_bonded = true,
        // Profiles 1 and 4 connected, 2 paired, 3 and 5 unpaired.
        .profiles_connected = {true, false, false, true, false},
        .profiles_bonded = {true, true, false, true, false},
        .layer_label = "QWERTY",
        .active_host = {.state = ROAMYBOARD_HOST_NAME_KNOWN, .name = "Alecto"},
#else
        .connected = true,
        .local_ota_hint = "hold the\n0 key 3 s\nfor OTA",
#endif
    };
    return state;
}

#if STATUS_SCREEN_CENTRAL

enum input_kind { END, KEY, MOUSE_DOWN, MOUSE_UP, POSE, PROFILE };

/** One event for the cat, at ms after the action begins. */
struct input {
    uint32_t ms;
    enum input_kind kind;
    int arg;
};

/** A row of the contact sheet: the cat's views from 0 to end_ms; inputs end at kind END. */
struct action {
    const char *name;
    int key_module_count;
    int x;
    uint32_t end_ms;
    /** Followed by at least one END. */
    struct input inputs[24];
};

#define T(ms) {ms, KEY, CAT_KEY_TYPING}
#define MAX_CELLS 24

static const struct action actions[] = {
    {"walk", 7, 8, 1800, {T(0), T(300), T(600), T(900), T(1200), T(1500)}},
    {"run", 7, 8, 1000, {T(0), T(100), T(200), T(300), T(400), T(500), T(600), T(700)}},
    {"space", 7, 8, 800, {T(0), {300, KEY, CAT_KEY_SPACE}}},
    {"enter", 7, 22, 700, {{0, KEY, CAT_KEY_ENTER}}},
    {"up no term", ROAMYBOARD_CHAIN_FAULT, 22, 700, {{0, KEY, CAT_KEY_UP}}},
    {"up ...", ROAMYBOARD_CHAIN_UNKNOWN, 22, 700, {{0, KEY, CAT_KEY_UP}}},
    {"moonwalk",
     7,
     30,
     1800,
     {T(0),
      {300, KEY, CAT_KEY_BACKSPACE},
      {600, KEY, CAT_KEY_BACKSPACE},
      {900, KEY, CAT_KEY_BACKSPACE},
      {1200, KEY, CAT_KEY_BACKSPACE},
      {1500, KEY, CAT_KEY_BACKSPACE}}},
    {"arrows",
     7,
     30,
     2200,
     {{0, KEY, CAT_KEY_LEFT},
      {300, KEY, CAT_KEY_LEFT},
      {600, KEY, CAT_KEY_RIGHT},
      {900, KEY, CAT_KEY_DOWN},
      {1500, KEY, CAT_KEY_UP}}},
    {"home end", 7, 30, 1800, {{0, KEY, CAT_KEY_HOME}, {800, KEY, CAT_KEY_END}}},
    {"wrap right",
     7,
     40,
     1500,
     {T(0), T(100), T(200), T(300), T(400), T(500), T(600), T(700), T(800), T(900), T(1000),
      T(1100), T(1200), T(1300)}},
    {"wrap left",
     7,
     10,
     1400,
     {{0, KEY, CAT_KEY_LEFT},
      {100, KEY, CAT_KEY_LEFT},
      {200, KEY, CAT_KEY_LEFT},
      {300, KEY, CAT_KEY_LEFT},
      {400, KEY, CAT_KEY_LEFT},
      {500, KEY, CAT_KEY_LEFT},
      {600, KEY, CAT_KEY_LEFT},
      {700, KEY, CAT_KEY_LEFT},
      {800, KEY, CAT_KEY_LEFT},
      {900, KEY, CAT_KEY_LEFT},
      {1000, KEY, CAT_KEY_LEFT},
      {1100, KEY, CAT_KEY_LEFT},
      {1200, KEY, CAT_KEY_LEFT}}},
    {"keypad", 7, 20, 1500, {{0, POSE, CAT_POSE_KEYPAD}, T(600)}},
    {"system",
     7,
     10,
     2600,
     {{0, POSE, CAT_POSE_SYSTEM},
      {300, MOUSE_DOWN, CAT_MOUSE_RIGHT},
      {1100, MOUSE_UP, CAT_MOUSE_RIGHT},
      {1400, MOUSE_DOWN, CAT_MOUSE_CLICK},
      {1450, MOUSE_UP, CAT_MOUSE_CLICK},
      {2100, MOUSE_DOWN, CAT_MOUSE_UP},
      {2400, MOUSE_UP, CAT_MOUSE_UP}}},
    {"gaming", 7, 20, 3400, {{0, POSE, CAT_POSE_GAMING}, {1600, KEY, CAT_KEY_OTHER}}},
    {"locked", 7, 50, 2600, {{0, POSE, CAT_POSE_LOCKED}, {1500, POSE, CAT_POSE_NONE}}},
    {"profile", 7, 30, 800, {{0, PROFILE, 0}}},
    {"idle", 7, 22, 32000, {T(0)}},
};

static void feed(struct cat *cat, const struct input *in) {
    switch (in->kind) {
    case KEY:
        cat_key_pressed(cat, (enum cat_key)in->arg, in->ms);
        break;
    case MOUSE_DOWN:
        cat_mouse_pressed(cat, (enum cat_mouse)in->arg, in->ms);
        break;
    case MOUSE_UP:
        cat_mouse_released(cat, (enum cat_mouse)in->arg, in->ms);
        break;
    case POSE:
        cat_set_pose(cat, (enum cat_pose)in->arg, in->ms);
        break;
    case PROFILE:
        cat_profile_changed(cat, in->ms);
        break;
    case END:
        break;
    }
}

/**
 * Renders every action as a row of scenes, one per change of the cat's view, labeled with
 * the action and the time in the host name's place, and lists them in cat-actions.txt.
 */
static void render_cat_actions(const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/cat-actions.txt", dir);
    FILE *index = fopen(path, "w");
    if (!index) {
        perror(path);
        exit(1);
    }
    for (size_t a = 0; a < sizeof(actions) / sizeof(actions[0]); a++) {
        const struct action *action = &actions[a];
        struct status_state state = base_state(action->key_module_count);
        struct cat cat;
        cat_init(&cat, action->x, 0);
        size_t next = 0;
        uint32_t t = 0;
        fprintf(index, "%s\t", action->name);
        for (int cell = 0; cell < MAX_CELLS && t < action->end_ms; cell++) {
            const struct input *in = action->inputs;
            while (in[next].kind != END && in[next].ms <= t) {
                feed(&cat, &in[next]);
                next++;
            }
            const struct cat_view view = cat_view_at(&cat, t);
            snprintf(state.active_host.name, sizeof(state.active_host.name), "%s\n%u ms",
                     action->name, (unsigned int)t);
            char name[64];
            snprintf(name, sizeof(name), "cat-%zu-%d", a, cell);
            render_cat(dir, name, &state, view.frame, view.x, view.y);
            fprintf(index, "%s%s", cell ? " " : "", name);

            t += view.valid_ms;
            if (action->inputs[next].kind != END && action->inputs[next].ms < t) {
                t = action->inputs[next].ms;
            }
        }
        fprintf(index, "\n");
    }
    fclose(index);
}

#endif

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
    render_cat(dir, "central-7cols-walk", &state, CAT_WALK_1, 14, 0);

    state = base_state(1);
    state.selected_endpoint.transport = ZMK_TRANSPORT_USB;
    state.charging = true;
    state.layer_label = "Keypad";
    state.active_profile_index = 3;
    snprintf(state.active_host.name, sizeof(state.active_host.name), "Nevyn's MacBook Pro");
    render_cat(dir, "central-1col-sit", &state, CAT_FACING_LEFT + CAT_SIT, 22, 0);

    state = base_state(0);
    state.active_profile_index = 1;
    state.active_profile_connected = false;
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_PENDING};
    render_cat(dir, "central-0cols-sleep", &state, CAT_SLEEP_1, 32, 0);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_ATT_ERROR,
                                                      .error = 0x0e};
    render_cat(dir, "central-noterm-blink", &state, CAT_BLINK, 2, 0);

    state = base_state(ROAMYBOARD_CHAIN_UNKNOWN);
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_READ_ERROR,
                                                      .error = -12};
    render_cat(dir, "central-unknown-walk", &state, CAT_WALK_0, CAT_BOX_WIDTH - 10, 0);

    state = base_state(7);
    state.active_profile_index = 2;
    state.active_profile_bonded = false;
    state.active_profile_connected = false;
    state.active_host = (struct roamyboard_host_name){.state = ROAMYBOARD_HOST_NAME_NONE};
    render_cat(dir, "central-unpaired-profile", &state, CAT_SIT, 10, 0);

    state = base_state(7);
    state.active_host.name[0] = '\0';
    render_cat(dir, "central-empty-name", &state, CAT_FACING_LEFT + CAT_WALK_2, -8, 0);

    state = base_state(7);
    snprintf(state.active_host.name, sizeof(state.active_host.name),
             "Living Room iPad Pro (12.9-inch) (6th generation)");
    render_cat(dir, "central-long-name", &state, CAT_WALK_3, 26, 0);

    state = base_state(7);
    state.pda_mode = ROAMYBOARD_PDA_HOME;
    state.show_count = false;
    render_cat(dir, "central-pda", &state, CAT_SIT, 10, 0);

    render_cat_actions(dir);

    render_bootloader(dir, "central-bootloader", ROAMYBOARD_BOOTLOADER_UF2, "3274ed3-dirty");
    render_bootloader(dir, "central-ota", ROAMYBOARD_BOOTLOADER_OTA, "3274ed3");
#else
    render_startup(dir, "peripheral-startup", base_state(7), "3274ed3");

    struct status_state state = base_state(7);
    render(dir, "peripheral-7cols", &state);

    state.connected = false;
    render(dir, "peripheral-no-central", &state);

    state = base_state(ROAMYBOARD_CHAIN_FAULT);
    state.connected = false;
    render(dir, "peripheral-noterm", &state);

    render_pda_scenes(dir);

    render_bootloader(dir, "peripheral-bootloader", ROAMYBOARD_BOOTLOADER_UF2, "3274ed3");
    render_bootloader(dir, "peripheral-ota", ROAMYBOARD_BOOTLOADER_OTA, "3274ed3-dirty");
#endif
    return 0;
}
