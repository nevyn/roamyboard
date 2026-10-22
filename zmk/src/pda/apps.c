/*
 * The PDA's small apps: Note, Keys and Info. docs/pda.md describes what each shows.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include <roamyboard/chain_state.h>

#include "keys.h"
#include "shell.h"

/* Note: a list of documents, and an editor for one of them. */

enum {
    USAGE_L = 0x0F,
    USAGE_N = 0x11,
    USAGE_SPACE = 0x2C,
};

static const char *const sort_names[PDA_DOCS_SORT_COUNT] = {"new", "edit", "A-Z"};

/** The list position of slot in order, or -1. */
static int index_of(const int8_t *order, int n, int slot) {
    for (int i = 0; i < n; i++) {
        if (order[i] == slot) {
            return i;
        }
    }
    return -1;
}

/** The highlighted slot, or the list's first when none is: the view can open before settings load. */
static int highlighted(const struct pda_shell *shell, const int8_t *order, int n) {
    const int selected = shell->note_app.selected;
    return selected >= 0 && shell->docs.docs[selected].used ? selected : n > 0 ? order[0] : -1;
}

static void open_doc(struct pda_shell *shell, int slot) {
    struct pda_note_app_state *a = &shell->note_app;
    a->open = (int8_t)slot;
    a->selected = (int8_t)slot;
    pda_note_reset_keys(&shell->docs.docs[slot].note);
    shell->docs.docs[slot].note.held_mods = shell->held_mods;
}

static void new_doc(struct pda_shell *shell) {
    const int slot = pda_docs_create(&shell->docs);
    if (slot < 0) {
        shell->note_app.open = -1;
        shell->note_app.full = true;
        return;
    }
    open_doc(shell, slot);
}

/** Deletes slot, highlighting the document after it in the list, or else the one before. */
static void delete_doc(struct pda_shell *shell, int slot) {
    struct pda_note_app_state *a = &shell->note_app;
    int8_t order[PDA_DOCS_MAX];
    const int n = pda_docs_order(&shell->docs, order);
    const int i = index_of(order, n, slot);
    a->selected = i + 1 < n ? order[i + 1] : i > 0 ? order[i - 1] : -1;
    if (a->open == slot) {
        a->open = -1;
    }
    pda_docs_delete(&shell->docs, slot);
}

/** Leaves the open document for the list; an empty one goes away. */
static void close_doc(struct pda_shell *shell) {
    const int slot = shell->note_app.open;
    if (shell->docs.docs[slot].note.len == 0) {
        delete_doc(shell, slot);
    }
    shell->note_app.open = -1;
}

static void note_open(struct pda_shell *shell) {
    struct pda_note_app_state *a = &shell->note_app;
    pda_docs_reset_keys(&shell->docs);
    a->open = -1;
    a->confirm = -1;
    a->full = false;
    if (shell->quick) {
        new_doc(shell);
    }
    int8_t order[PDA_DOCS_MAX];
    a->selected = (int8_t)highlighted(shell, order, pda_docs_order(&shell->docs, order));
}

static enum pda_app_result doc_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                   bool cmd) {
    struct pda_note_app_state *a = &shell->note_app;
    const int slot = a->open;
    struct pda_note *note = &shell->docs.docs[slot].note;
    if (cmd && usage == PDA_USAGE_BACKSPACE) {
        a->confirm = (int8_t)slot;
        return PDA_APP_REDRAW;
    }
    switch (pda_note_key(note, usage, implicit_mods, true)) {
    case PDA_NOTE_NONE:
        return PDA_APP_NONE;
    case PDA_NOTE_CHANGED:
        pda_docs_touch(&shell->docs, slot);
        return PDA_APP_REDRAW;
    case PDA_NOTE_TYPE_OUT:
        memcpy(shell->type_out, note->text, note->len);
        shell->type_out_len = note->len;
        return PDA_APP_TYPE_OUT;
    case PDA_NOTE_LEAVE:
        close_doc(shell);
        // A quick note leaves PDA mode; otherwise Escape goes back to the list.
        return shell->quick ? PDA_APP_CLOSE : PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result list_key(struct pda_shell *shell, uint8_t usage, bool cmd) {
    struct pda_note_app_state *a = &shell->note_app;
    int8_t order[PDA_DOCS_MAX];
    const int n = pda_docs_order(&shell->docs, order);
    a->selected = (int8_t)highlighted(shell, order, n);
    const int i = index_of(order, n, a->selected);
    if (cmd) {
        if (usage == USAGE_L) {
            shell->docs.sort = (uint8_t)((shell->docs.sort + 1) % PDA_DOCS_SORT_COUNT);
            shell->docs.sort_changed = true;
            return PDA_APP_REDRAW;
        }
        if (usage == PDA_USAGE_BACKSPACE && a->selected >= 0) {
            a->confirm = a->selected;
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    if ((usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE ||
         pda_key_dir(usage) == PDA_DIR_RIGHT) &&
        a->selected >= 0) {
        open_doc(shell, a->selected);
        return PDA_APP_REDRAW;
    }
    const enum pda_dir dir = pda_key_dir(usage);
    if (n > 0 && (dir == PDA_DIR_UP || dir == PDA_DIR_DOWN)) {
        const int next = i < 0 ? 0 : dir == PDA_DIR_UP ? (i > 0 ? i - 1 : 0) : (i + 1 < n ? i + 1 : i);
        a->selected = order[next];
        return next != i ? PDA_APP_REDRAW : PDA_APP_NONE;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result note_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    struct pda_note_app_state *a = &shell->note_app;
    const bool modifier = usage >= PDA_USAGE_LEFT_CONTROL && usage <= PDA_USAGE_RIGHT_GUI;
    if (modifier || !pressed) {
        if (a->open >= 0) {
            pda_note_key(&shell->docs.docs[a->open].note, usage, implicit_mods, pressed);
        }
        return PDA_APP_NONE;
    }
    if (a->confirm >= 0) {
        if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER) {
            delete_doc(shell, a->confirm);
        }
        a->confirm = -1;
        return PDA_APP_REDRAW;
    }
    const bool was_full = a->full;
    a->full = false;
    const bool cmd = (shell->held_mods | implicit_mods) & PDA_MODS_GUI;
    if (cmd && usage == USAGE_N) {
        if (a->open >= 0) {
            close_doc(shell);
        }
        new_doc(shell);
        return PDA_APP_REDRAW;
    }
    const enum pda_app_result result =
        a->open >= 0 ? doc_key(shell, usage, implicit_mods, cmd) : list_key(shell, usage, cmd);
    return was_full && result == PDA_APP_NONE ? PDA_APP_REDRAW : result;
}

#define NOTE_MAX_LINES 96

/** The text with a cursor, wrapped; the last lines that fit, as on a receipt. */
static void draw_doc(const struct pda_note *note, struct pda_fb *fb, struct pda_rect area) {
    static char buf[PDA_NOTE_MAX + 2];
    static char line[PDA_NOTE_MAX + 2];
    static size_t starts[NOTE_MAX_LINES + 1];
    snprintf(buf, sizeof(buf), "%s_", note->text);
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

/** A row per document, the highlighted one inverted, scrolled to keep it in view. */
static void draw_list(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    int8_t order[PDA_DOCS_MAX];
    const int n = pda_docs_order(&shell->docs, order);
    char text[24];
    const int small_h = pda_text_line_height(PDA_FONT_SMALL);
    pda_text_draw(fb, area.x + 1, area.y, 40, sort_names[shell->docs.sort], PDA_FONT_SMALL, true);
    snprintf(text, sizeof(text), "%d/%d", n, PDA_DOCS_MAX);
    const int count_w = pda_text_width(text, PDA_FONT_SMALL);
    pda_text_draw(fb, area.x + area.w - count_w - 1, area.y, count_w + 1, text, PDA_FONT_SMALL,
                  true);
    const int top = area.y + small_h + 1;
    pda_fb_fill(fb, (struct pda_rect){area.x, top - 1, area.w, 1}, true);

    if (n == 0) {
        pda_text_center(fb, area, top + 30, "No notes", PDA_FONT_BODY, true);
        pda_text_center(fb, area, top + 50, "? help", PDA_FONT_SMALL, true);
        return;
    }
    const int row_h = pda_text_line_height(PDA_FONT_BODY) + 2;
    const int visible = (area.y + area.h - top) / row_h;
    const int selected = highlighted(shell, order, n);
    const int i = index_of(order, n, selected);
    const int first = i >= visible ? i - visible + 1 : 0;
    static char title[PDA_NOTE_MAX + 1];
    for (int row = 0; row < visible && first + row < n; row++) {
        const int slot = order[first + row];
        const int y = top + 1 + row * row_h;
        pda_docs_title(&shell->docs, slot, title, sizeof(title));
        const size_t fit = pda_text_line_len(title, strlen(title), PDA_FONT_BODY, area.w - 4);
        title[fit] = '\0';
        pda_text_draw(fb, area.x + 2, y + 1, area.w - 4, title[0] ? title : "Untitled",
                      PDA_FONT_BODY, true);
        if (slot == selected) {
            pda_fb_invert(fb, (struct pda_rect){area.x, y, area.w, row_h});
        }
    }
    if (first + visible < n) {
        pda_fb_fill(fb, (struct pda_rect){area.x + area.w - 2, area.y + area.h - 6, 2, 6}, true);
    }
}

static void note_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_note_app_state *a = &shell->note_app;
    if (a->open >= 0) {
        draw_doc(&shell->docs.docs[a->open].note, fb, area);
    } else {
        draw_list(shell, fb, area);
    }
    if (a->confirm >= 0) {
        static char title[PDA_NOTE_MAX + 1];
        pda_docs_title(&shell->docs, a->confirm, title, sizeof(title));
        const size_t fit = pda_text_line_len(title, strlen(title), PDA_FONT_BODY, area.w - 8);
        title[fit] = '\0';
        const char *const lines[3] = {"Delete", title[0] ? title : "Untitled", "ret: yes"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (a->full) {
        char full[16];
        snprintf(full, sizeof(full), "%d notes:", PDA_DOCS_MAX);
        const char *const lines[3] = {full, "delete one", "first"};
        pda_game_banner(fb, area, area.y + 40, lines);
    }
}

static const char note_help[] =
    "In the list\n"
    "Up, Down (W S, K J)\tPick a note.\n"
    "Return, Space, Right\tOpen it.\n"
    "Cmd-N\tA new note.\n"
    "Cmd-L\tSort by newest, last edited, or title.\n"
    "Cmd-Backspace\tDelete the note; Return confirms.\n"
    "Esc\tBack to the home screen.\n"
    "In a note\n"
    "Return\tA new line.\n"
    "Backspace\tDelete the last character.\n"
    "Cmd-Return\tType the note out to the computer.\n"
    "Cmd-N\tA new note.\n"
    "Cmd-Backspace\tDelete the note; Return confirms.\n"
    "Esc\tBack to the list; an empty note goes away.\n"
    "å ä ö\tOn the Keypad layer.";

/** In a note, ? is text; in the list, it shows the help. */
static bool note_types_question_mark(const struct pda_shell *shell) {
    return shell->note_app.open >= 0;
}

const struct pda_app pda_app_note_def = {
    .name = "Note",
    .open = note_open,
    .key = note_key,
    .draw = note_draw,
    .help = note_help,
    .types_question_mark = note_types_question_mark,
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

static enum pda_app_result keys_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    (void)implicit_mods;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    // Cmd-Backspace starts over, for testing another build.
    if (usage == PDA_USAGE_BACKSPACE && (shell->held_mods & PDA_MODS_GUI)) {
        memset(&shell->keys, 0, sizeof(shell->keys));
        shell->keys.last_position = -1;
        shell->keys.last_usage = -1;
        return PDA_APP_REDRAW;
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

static const char keys_help[] =
    "Press keys to see them: filled while held, outlined once pressed.\n"
    "Cmd-Backspace\tStart over.\n"
    "Esc\tBack to the home screen.";

const struct pda_app pda_app_keys_def = {
    .name = "Keys",
    .key = keys_key,
    .position = keys_position,
    .draw = keys_draw,
    .help = keys_help,
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

static const char info_help[] =
    "Each half's key module count, battery and build ID, and where the clock comes from.\n"
    "Esc\tBack to the home screen.";

const struct pda_app pda_app_info_def = {
    .name = "Info",
    .key = info_key,
    .draw = info_draw,
    .help = info_help,
};
