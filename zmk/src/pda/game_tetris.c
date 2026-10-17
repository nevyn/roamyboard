/*
 * Tetris: a 10 x 20 well of 6-pixel cells, the seven tetrominoes dealt from shuffled bags,
 * rotation with wall kicks, and the classic line scores times the level.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

#define CELL 6
/** Ticks while playing; each tick checks whether the piece is due to fall. */
#define PLAY_TICK_MS 50
#define PIECES 7

enum {
    USAGE_P = 0x13,
    USAGE_X = 0x1B,
    USAGE_Z = 0x1D,
    USAGE_SPACE = 0x2C,
};

/** Each piece's cells in rotation 0, inside a box of box[piece] cells; I O T S Z J L. */
static const int8_t shapes[PIECES][4][2] = {
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}}, {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
};
static const int8_t box[PIECES] = {4, 2, 3, 3, 3, 3, 3};

/** Cell i of piece in rotation, turned clockwise inside its box. */
static void cell(int piece, int rotation, int i, int *x, int *y) {
    int cx = shapes[piece][i][0], cy = shapes[piece][i][1];
    for (int r = 0; r < rotation; r++) {
        const int nx = box[piece] - 1 - cy;
        cy = cx;
        cx = nx;
    }
    *x = cx;
    *y = cy;
}

static bool fits(const struct pda_tetris_state *t, int piece, int rotation, int x, int y) {
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        cell(piece, rotation, i, &cx, &cy);
        cx += x;
        cy += y;
        if (cx < 0 || cx >= PDA_TETRIS_W || cy >= PDA_TETRIS_H) {
            return false;
        }
        if (cy >= 0 && t->board[cy][cx]) {
            return false;
        }
    }
    return true;
}

static uint8_t deal(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    if (t->bag_left == 0) {
        for (int i = 0; i < PIECES; i++) {
            t->bag[i] = (uint8_t)i;
        }
        for (int i = PIECES - 1; i > 0; i--) {
            const int j = (int)pda_shell_random(shell, (uint32_t)i + 1);
            const uint8_t swap = t->bag[i];
            t->bag[i] = t->bag[j];
            t->bag[j] = swap;
        }
        t->bag_left = PIECES;
    }
    return t->bag[--t->bag_left];
}

static uint32_t fall_ms(const struct pda_tetris_state *t) {
    const int ms = 800 - 70 * t->level;
    return ms < 80 ? 80 : (uint32_t)ms;
}

static void game_over(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    t->over = true;
    shell->tick_ms = 0;
    if (t->score > shell->scores.tetris) {
        shell->scores.tetris = t->score;
        shell->scores_changed = true;
    }
}

/** Brings the next piece in at the top; the game ends if it does not fit. */
static void spawn(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    t->piece = t->next;
    t->next = deal(shell);
    t->rotation = 0;
    t->x = (int8_t)((PDA_TETRIS_W - box[t->piece]) / 2);
    t->y = t->piece == 0 ? -1 : 0;
    t->last_fall_ms = shell->now_ms;
    if (!fits(t, t->piece, t->rotation, t->x, t->y)) {
        game_over(shell);
    }
}

static void start(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    memset(t, 0, sizeof(*t));
    t->started = true;
    t->next = deal(shell);
    spawn(shell);
    shell->tick_ms = PLAY_TICK_MS;
}

static void clear_lines(struct pda_shell *shell) {
    static const uint16_t points[5] = {0, 100, 300, 500, 800};
    struct pda_tetris_state *t = &shell->tetris;
    int cleared = 0;
    for (int row = PDA_TETRIS_H - 1; row >= 0; row--) {
        bool full = true;
        for (int x = 0; x < PDA_TETRIS_W; x++) {
            full = full && t->board[row][x];
        }
        if (!full) {
            continue;
        }
        memmove(t->board[1], t->board[0], (size_t)row * PDA_TETRIS_W);
        memset(t->board[0], 0, PDA_TETRIS_W);
        cleared++;
        row++; // the row that moved down into this one
    }
    t->score += points[cleared] * (t->level + 1u);
    t->lines += (uint32_t)cleared;
    t->level = (uint8_t)(t->lines / 10);
}

/** Fixes the piece in the well, clears full lines and brings the next piece. */
static void lock(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        cell(t->piece, t->rotation, i, &cx, &cy);
        if (t->y + cy < 0) {
            game_over(shell);
            return;
        }
        t->board[t->y + cy][t->x + cx] = (uint8_t)(t->piece + 1);
    }
    clear_lines(shell);
    spawn(shell);
}

/** Moves the piece down one row, or locks it. */
static void fall(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    if (fits(t, t->piece, t->rotation, t->x, t->y + 1)) {
        t->y++;
    } else {
        lock(shell);
    }
    t->last_fall_ms = shell->now_ms;
}

static void rotate(struct pda_tetris_state *t, int turns) {
    static const int8_t kicks[][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};
    const int rotation = (t->rotation + turns) % 4;
    for (size_t i = 0; i < sizeof(kicks) / sizeof(kicks[0]); i++) {
        if (fits(t, t->piece, rotation, t->x + kicks[i][0], t->y + kicks[i][1])) {
            t->rotation = (uint8_t)rotation;
            t->x = (int8_t)(t->x + kicks[i][0]);
            t->y = (int8_t)(t->y + kicks[i][1]);
            return;
        }
    }
}

static void tetris_open(struct pda_shell *shell) {
    const struct pda_tetris_state *t = &shell->tetris;
    shell->tick_ms = t->started && !t->over && !t->paused ? PLAY_TICK_MS : 0;
}

static enum pda_app_result tetris_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                      bool pressed) {
    (void)implicit_mods;
    struct pda_tetris_state *t = &shell->tetris;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        t->paused = t->started && !t->over;
        return PDA_APP_CLOSE;
    }
    if (!t->started || t->over) {
        if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE) {
            start(shell);
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    if (usage == USAGE_P || t->paused) {
        t->paused = !t->paused;
        t->last_fall_ms = shell->now_ms;
        shell->tick_ms = t->paused ? 0 : PLAY_TICK_MS;
        return PDA_APP_REDRAW;
    }
    switch (pda_key_dir(usage)) {
    case PDA_DIR_LEFT:
        if (fits(t, t->piece, t->rotation, t->x - 1, t->y)) {
            t->x--;
        }
        return PDA_APP_REDRAW;
    case PDA_DIR_RIGHT:
        if (fits(t, t->piece, t->rotation, t->x + 1, t->y)) {
            t->x++;
        }
        return PDA_APP_REDRAW;
    case PDA_DIR_DOWN:
        if (fits(t, t->piece, t->rotation, t->x, t->y + 1)) {
            t->score++;
        }
        fall(shell);
        return PDA_APP_REDRAW;
    case PDA_DIR_UP:
        rotate(t, 1);
        return PDA_APP_REDRAW;
    case PDA_DIR_NONE:
        break;
    }
    switch (usage) {
    case USAGE_X:
        rotate(t, 1);
        return PDA_APP_REDRAW;
    case USAGE_Z:
        rotate(t, 3);
        return PDA_APP_REDRAW;
    case USAGE_SPACE:
        while (fits(t, t->piece, t->rotation, t->x, t->y + 1)) {
            t->y++;
            t->score += 2;
        }
        lock(shell);
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result tetris_tick(struct pda_shell *shell) {
    struct pda_tetris_state *t = &shell->tetris;
    if (!t->started || t->over || t->paused) {
        t->last_fall_ms = shell->now_ms;
        return PDA_APP_NONE;
    }
    if (shell->now_ms - t->last_fall_ms < fall_ms(t)) {
        return PDA_APP_NONE;
    }
    fall(shell);
    return PDA_APP_REDRAW;
}

static void draw_cell(struct pda_fb *fb, int x, int y, int size) {
    pda_fb_fill(fb, (struct pda_rect){x, y, size - 1, size - 1}, true);
}

static void tetris_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_tetris_state *t = &shell->tetris;
    const int bx = area.x + (area.w - PDA_TETRIS_W * CELL) / 2;
    const int by = area.y + 14;
    pda_fb_frame(fb, (struct pda_rect){bx - 2, by - 2, PDA_TETRIS_W * CELL + 3,
                                       PDA_TETRIS_H * CELL + 3}, true);

    char text[24];
    snprintf(text, sizeof(text), "%lu", (unsigned long)t->score);
    pda_text_draw(fb, area.x + 2, area.y + 2, 44, text, PDA_FONT_SMALL, true);

    for (int y = 0; y < PDA_TETRIS_H; y++) {
        for (int x = 0; x < PDA_TETRIS_W; x++) {
            if (t->board[y][x]) {
                draw_cell(fb, bx + x * CELL, by + y * CELL, CELL);
            }
        }
    }
    if (t->started && !t->over) {
        for (int i = 0; i < 4; i++) {
            int cx, cy;
            cell(t->piece, t->rotation, i, &cx, &cy);
            if (t->y + cy >= 0) {
                draw_cell(fb, bx + (t->x + cx) * CELL, by + (t->y + cy) * CELL, CELL);
            }
        }
        // The next piece, small, top right.
        for (int i = 0; i < 4; i++) {
            int cx, cy;
            cell(t->next, 0, i, &cx, &cy);
            draw_cell(fb, area.x + area.w - 14 + cx * 3, area.y + 2 + cy * 3, 3);
        }
    }

    char best[24];
    snprintf(best, sizeof(best), "best %lu", (unsigned long)shell->scores.tetris);
    if (!t->started) {
        const char *const lines[3] = {"Tetris", best, "spc: play"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (t->over) {
        const char *const lines[3] = {"game over", best, "spc: again"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (t->paused || shell->mode == ROAMYBOARD_PDA_OFF) {
        const char *const lines[3] = {"paused", NULL, NULL};
        pda_game_banner(fb, area, area.y + 50, lines);
    }
}

const struct pda_app pda_app_tetris_def = {
    .name = "Tetris",
    .open = tetris_open,
    .key = tetris_key,
    .tick = tetris_tick,
    .focused_ticks = true,
    .draw = tetris_draw,
};
