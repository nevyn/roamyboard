/*
 * Anarch, a Doom-like by drummyfish, on the whole display, sideways: the game's top is the
 * display's right edge. The runner in anarch.h runs the game; this app feeds it the keys and
 * shows its frames.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "keys.h"
#include "shell.h"

#define TICK_MS 20

_Static_assert(PDA_ANARCH_W == PDA_FB_H && PDA_ANARCH_H == PDA_FB_W,
               "the game's frame fills the display sideways");

enum {
    USAGE_A = 0x04,
    USAGE_C = 0x06,
    USAGE_D = 0x07,
    USAGE_E = 0x08,
    USAGE_J = 0x0D,
    USAGE_M = 0x10,
    USAGE_Q = 0x14,
    USAGE_S = 0x16,
    USAGE_W = 0x1A,
    USAGE_SPACE = 0x2C,
    USAGE_RIGHT = 0x4F,
    USAGE_LEFT = 0x50,
    USAGE_DOWN = 0x51,
    USAGE_UP = 0x52,
    USAGE_LEFT_SHIFT = 0xE1,
    USAGE_RIGHT_SHIFT = 0xE5,
};

/** The game key that usage holds, or -1. */
static int key_for(uint8_t usage) {
    switch (usage) {
    case USAGE_UP:
    case USAGE_W:
        return PDA_ANARCH_KEY_UP;
    case USAGE_DOWN:
    case USAGE_S:
        return PDA_ANARCH_KEY_DOWN;
    case USAGE_LEFT:
        return PDA_ANARCH_KEY_LEFT;
    case USAGE_RIGHT:
        return PDA_ANARCH_KEY_RIGHT;
    case USAGE_A:
        return PDA_ANARCH_KEY_STRAFE_LEFT;
    case USAGE_D:
        return PDA_ANARCH_KEY_STRAFE_RIGHT;
    case USAGE_SPACE:
    case PDA_USAGE_RETURN:
    case PDA_USAGE_KP_ENTER:
        return PDA_ANARCH_KEY_A;
    case USAGE_LEFT_SHIFT:
    case USAGE_RIGHT_SHIFT:
    case PDA_USAGE_BACKSPACE:
        return PDA_ANARCH_KEY_B;
    case USAGE_C:
        return PDA_ANARCH_KEY_C;
    case USAGE_J:
        return PDA_ANARCH_KEY_JUMP;
    case PDA_USAGE_TAB:
        return PDA_ANARCH_KEY_MAP;
    case USAGE_Q:
        return PDA_ANARCH_KEY_PREVIOUS_WEAPON;
    case USAGE_E:
        return PDA_ANARCH_KEY_NEXT_WEAPON;
    case USAGE_M:
        return PDA_ANARCH_KEY_MENU;
    }
    return -1;
}

static uint16_t key_mask(const struct pda_anarch_state *s) {
    uint16_t mask = 0;
    for (int k = 0; k < PDA_ANARCH_KEY_COUNT; k++) {
        if (s->held[k]) {
            mask |= (uint16_t)(1u << k);
        }
    }
    return mask;
}

static void anarch_open(struct pda_shell *shell) {
    memset(shell->anarch.held, 0, sizeof(shell->anarch.held));
    shell->tick_ms = TICK_MS;
}

static enum pda_app_result anarch_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                      bool pressed) {
    (void)implicit_mods;
    struct pda_anarch_state *s = &shell->anarch;
    if (usage == PDA_USAGE_ESCAPE) {
        return pressed ? PDA_APP_CLOSE : PDA_APP_NONE;
    }
    const int key = key_for(usage);
    if (key < 0) {
        return PDA_APP_NONE;
    }
    // Two usages can hold one key, such as Up and W; it stays down until both are released.
    if (pressed) {
        s->held[key]++;
    } else if (s->held[key] > 0) {
        s->held[key]--;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result anarch_tick(struct pda_shell *shell) {
    struct pda_anarch_state *s = &shell->anarch;
    // Without ticks the PDA had no focus, and the releases meanwhile went to the host.
    if (shell->now_ms - s->last_tick_ms > PDA_ANARCH_LEASE_MS) {
        memset(s->held, 0, sizeof(s->held));
    }
    s->last_tick_ms = shell->now_ms;
    pda_anarch_feed(shell->now_ms, key_mask(s));
    if (!pda_anarch_take_frame(s->frame)) {
        return PDA_APP_NONE;
    }
    s->has_frame = true;
    return PDA_APP_REDRAW;
}

static void anarch_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_anarch_state *s = &shell->anarch;
    const bool paused = shell->mode == ROAMYBOARD_PDA_OFF;
    if (s->has_frame) {
        // Unfocused, the display shows lit pixels dark; inverting keeps the picture the same.
        for (int gy = 0; gy < PDA_ANARCH_H; gy++) {
            const uint8_t *row = s->frame + gy * PDA_ANARCH_STRIDE;
            for (int gx = 0; gx < PDA_ANARCH_W; gx++) {
                const bool light = row[gx / 8] & (0x80 >> (gx % 8));
                pda_fb_set(fb, PDA_FB_W - 1 - gy, gx, light != paused);
            }
        }
    }
    if (!s->has_frame || paused) {
        const char *const lines[3] = {s->has_frame ? "paused" : "Anarch",
                                      s->has_frame || !paused ? NULL : "paused", NULL};
        pda_game_banner(fb, area, area.y + 60, lines);
    }
}

static const char anarch_help[] =
    "Up, Down, W S\tWalk.\n"
    "Left, Right\tTurn.\n"
    "A D\tStrafe.\n"
    "Space, Return\tFire; in the menu, choose.\n"
    "Shift, Backspace\tIn the menu, back. Held, Left and Right strafe.\n"
    "Q E\tPrevious and next weapon.\n"
    "Tab\tThe map, while held.\n"
    "J\tJump.\n"
    "M\tThe game's menu.\n"
    "The display shows the game sideways: its top is the right edge.\n"
    "Esc\tBack to the home screen; the game waits.";

const struct pda_app pda_app_anarch_def = {
    .name = "Anarch",
    .open = anarch_open,
    .key = anarch_key,
    .tick = anarch_tick,
    .focused_ticks = true,
    .fullscreen = true,
    .draw = anarch_draw,
    .help = anarch_help,
};
