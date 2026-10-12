/*
 * The PDA's apps: Note, Keys and Info. docs/pda.md describes what each shows.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include <roamyboard/chain_state.h>

#include "keys.h"
#include "shell.h"

/* Note */

static void note_open(struct pda_shell *shell) { pda_note_reset_keys(&shell->note); }

static enum pda_app_result note_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    switch (pda_note_key(&shell->note, usage, implicit_mods, pressed)) {
    case PDA_NOTE_NONE:
        return PDA_APP_NONE;
    case PDA_NOTE_CHANGED:
        return PDA_APP_REDRAW;
    case PDA_NOTE_TYPE_OUT:
        return PDA_APP_TYPE_OUT;
    case PDA_NOTE_LEAVE:
        return PDA_APP_CLOSE;
    }
    return PDA_APP_NONE;
}

#define NOTE_MAX_LINES 96

/** The text with a cursor, wrapped; the last lines that fit, as on a receipt. */
static void note_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    static char buf[PDA_NOTE_MAX + 2];
    static char line[PDA_NOTE_MAX + 2];
    static size_t starts[NOTE_MAX_LINES + 1];
    snprintf(buf, sizeof(buf), "%s_", shell->note.text);
    const size_t len = strlen(buf);
    const int x = area.x + 1;
    const int w = area.w - 2;

    int lines = 0;
    for (size_t pos = 0; pos < len && lines < NOTE_MAX_LINES;) {
        starts[lines++] = pos;
        pos += pda_text_line_len(buf + pos, len - pos, PDA_FONT_BODY, w);
    }
    starts[lines] = len;

    const int line_h = pda_text_line_height(PDA_FONT_BODY);
    const int visible = (area.h - 1) / line_h;
    const int first = lines > visible ? lines - visible : 0;
    for (int i = first; i < lines; i++) {
        size_t n = starts[i + 1] - starts[i];
        memcpy(line, buf + starts[i], n);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' ')) {
            n--;
        }
        line[n] = '\0';
        pda_text_draw(fb, x, area.y + 1 + (i - first) * line_h, w, line, PDA_FONT_BODY, true);
    }
}

const struct pda_app pda_app_note_def = {
    .name = "Note",
    .open = note_open,
    .key = note_key,
    .draw = note_draw,
};

/* Keys: a cell per key position, keymap columns top to bottom and rows left to right. */

#define KEYS_COLUMNS 24
#define KEYS_ROWS 5
#define KEYS_CELL_W 12
#define KEYS_CELL_H 4
#define KEYS_GAP 1
/** Extra space between the halves, after keymap column 11. */
#define KEYS_HALF_GAP 3

static bool bit(const uint8_t *map, int position) { return map[position / 8] & (1 << (position % 8)); }

static void set_bit(uint8_t *map, int position, bool on) {
    const uint8_t mask = (uint8_t)(1 << (position % 8));
    map[position / 8] = on ? (map[position / 8] | mask) : (map[position / 8] & ~mask);
}

static void keys_open(struct pda_shell *shell) {
    memset(&shell->keys, 0, sizeof(shell->keys));
    shell->keys.last_position = -1;
    shell->keys.last_usage = -1;
}

static enum pda_app_result keys_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    (void)implicit_mods;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    shell->keys.last_usage = usage;
    return PDA_APP_REDRAW;
}

static enum pda_app_result keys_position(struct pda_shell *shell, uint8_t position, bool pressed) {
    if (position >= KEYS_COLUMNS * KEYS_ROWS) {
        return PDA_APP_NONE;
    }
    set_bit(shell->keys.held, position, pressed);
    if (pressed) {
        set_bit(shell->keys.seen, position, true);
        shell->keys.last_position = position;
        // The keycode, if the key has one, arrives after its position.
        shell->keys.last_usage = -1;
    }
    return PDA_APP_REDRAW;
}

static void keys_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    for (int column = 0; column < KEYS_COLUMNS; column++) {
        const int y = area.y + 1 + column * (KEYS_CELL_H + KEYS_GAP) +
                      (column >= KEYS_COLUMNS / 2 ? KEYS_HALF_GAP : 0);
        for (int row = 0; row < KEYS_ROWS; row++) {
            const int position = row * KEYS_COLUMNS + column;
            const struct pda_rect cell = {area.x + 1 + row * (KEYS_CELL_W + KEYS_GAP), y,
                                          KEYS_CELL_W, KEYS_CELL_H};
            if (bit(shell->keys.held, position)) {
                pda_fb_fill(fb, cell, true);
            } else if (bit(shell->keys.seen, position)) {
                pda_fb_frame(fb, cell, true);
            } else {
                pda_fb_set(fb, cell.x + cell.w / 2, cell.y + cell.h / 2, true);
            }
        }
    }

    char text[24];
    const int position = shell->keys.last_position;
    if (position < 0) {
        snprintf(text, sizeof(text), "press keys");
    } else if (shell->keys.last_usage < 0) {
        snprintf(text, sizeof(text), "r%dc%d", position / KEYS_COLUMNS + 1,
                 position % KEYS_COLUMNS);
    } else {
        snprintf(text, sizeof(text), "r%dc%d %02X", position / KEYS_COLUMNS + 1,
                 position % KEYS_COLUMNS, shell->keys.last_usage);
    }
    const int text_y = area.y + area.h - pda_text_line_height(PDA_FONT_SMALL) - 1;
    pda_text_draw(fb, area.x + 1, text_y, area.w - 2, text, PDA_FONT_SMALL, true);
}

const struct pda_app pda_app_keys_def = {
    .name = "Keys",
    .open = keys_open,
    .key = keys_key,
    .position = keys_position,
    .draw = keys_draw,
};

/* Info */

static enum pda_app_result info_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    (void)shell;
    (void)implicit_mods;
    return pressed && usage == PDA_USAGE_ESCAPE ? PDA_APP_CLOSE : PDA_APP_NONE;
}

static int draw_half(struct pda_fb *fb, int y, const char *name, const struct pda_half_info *half) {
    const int body_h = pda_text_line_height(PDA_FONT_BODY);
    pda_text_draw(fb, 1, y, PDA_FB_W - 2, name, PDA_FONT_TITLE, true);
    y += pda_text_line_height(PDA_FONT_TITLE);
    if (!half->known) {
        pda_text_draw(fb, 1, y, PDA_FB_W - 2, "...", PDA_FONT_BODY, true);
        return y + body_h;
    }
    char count[12];
    switch (half->key_module_count) {
    case ROAMYBOARD_CHAIN_UNKNOWN:
        snprintf(count, sizeof(count), "...");
        break;
    case ROAMYBOARD_CHAIN_FAULT:
        snprintf(count, sizeof(count), "no term");
        break;
    default:
        snprintf(count, sizeof(count), "%d col%s", half->key_module_count,
                 half->key_module_count == 1 ? "" : "s");
        break;
    }
    char text[24];
    snprintf(text, sizeof(text), "%s %u%%", count, half->battery);
    pda_text_draw(fb, 1, y, PDA_FB_W - 2, text, PDA_FONT_BODY, true);
    y += body_h + 2;
    // A "-dirty" suffix wraps to its own line, as on the status screen.
    const int small_h = pda_text_line_height(PDA_FONT_SMALL);
    snprintf(text, sizeof(text), "%s", half->build_id);
    char *dash = strchr(text, '-');
    if (dash != NULL && pda_text_width(text, PDA_FONT_SMALL) > PDA_FB_W - 2) {
        *dash = '\0';
        pda_text_draw(fb, 1, y, PDA_FB_W - 2, text, PDA_FONT_SMALL, true);
        y += small_h;
        *dash = '-';
        pda_text_draw(fb, 1, y, PDA_FB_W - 2, dash, PDA_FONT_SMALL, true);
    } else {
        pda_text_draw(fb, 1, y, PDA_FB_W - 2, text, PDA_FONT_SMALL, true);
    }
    return y + small_h;
}

static void info_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    int y = draw_half(fb, area.y + 2, "Left", &shell->left);
    y = draw_half(fb, y + 6, "Right", &shell->right);

    // Where the clock comes from: the profile (1 to 5) whose host gave the time.
    char text[24];
    switch (shell->clock.status) {
    case PDA_CLOCK_UNKNOWN:
        snprintf(text, sizeof(text), "time ...");
        break;
    case PDA_CLOCK_SET:
        snprintf(text, sizeof(text), "time BT%u", shell->clock.profile + 1);
        break;
    case PDA_CLOCK_NO_CTS:
        snprintf(text, sizeof(text), "no CTS");
        break;
    case PDA_CLOCK_ERROR:
        snprintf(text, sizeof(text), "time err");
        break;
    }
    pda_text_draw(fb, 1, y + 4, area.w - 2, text, PDA_FONT_SMALL, true);
}

const struct pda_app pda_app_info_def = {
    .name = "Info",
    .key = info_key,
    .draw = info_draw,
};
