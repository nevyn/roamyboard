/*
 * Snake: a 17 x 32 field of 4-pixel cells, walled by the display's edges. Each piece of food
 * grows the snake by two cells and speeds it up.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

#define CELL 4
#define CELLS (PDA_SNAKE_W * PDA_SNAKE_H)
#define PLAY_TICK_MS 20

enum {
    USAGE_P = 0x13,
    USAGE_SPACE = 0x2C,
};

static uint32_t step_ms(const struct pda_snake_state *s) {
    const int ms = 160 - 2 * s->len;
    return ms < 70 ? 70 : (uint32_t)ms;
}

static uint16_t body_at(const struct pda_snake_state *s, int i) {
    return s->body[(s->head + CELLS - i) % CELLS];
}

/** Whether cell is part of the snake; the tail counts only if skip_tail is false. */
static bool on_snake(const struct pda_snake_state *s, uint16_t cell, bool skip_tail) {
    const int len = skip_tail ? s->len - 1 : s->len;
    for (int i = 0; i < len; i++) {
        if (body_at(s, i) == cell) {
            return true;
        }
    }
    return false;
}

static void place_food(struct pda_shell *shell) {
    struct pda_snake_state *s = &shell->snake;
    for (int tries = 0; tries < 64; tries++) {
        const uint16_t cell = (uint16_t)pda_shell_random(shell, CELLS);
        if (!on_snake(s, cell, false)) {
            s->food = cell;
            return;
        }
    }
    // A long snake: take the first free cell after a random start.
    const uint16_t start = (uint16_t)pda_shell_random(shell, CELLS);
    for (int i = 0; i < CELLS; i++) {
        const uint16_t cell = (uint16_t)((start + i) % CELLS);
        if (!on_snake(s, cell, false)) {
            s->food = cell;
            return;
        }
    }
}

static void start(struct pda_shell *shell) {
    struct pda_snake_state *s = &shell->snake;
    memset(s, 0, sizeof(*s));
    s->started = true;
    s->len = 3;
    const int y = PDA_SNAKE_H / 2;
    for (int i = 0; i < 3; i++) {
        s->body[i] = (uint16_t)(y * PDA_SNAKE_W + 6 + i);
    }
    s->head = 2;
    s->dir = s->next_dir = PDA_DIR_RIGHT;
    place_food(shell);
    s->last_step_ms = shell->now_ms;
    shell->tick_ms = PLAY_TICK_MS;
}

static void game_over(struct pda_shell *shell) {
    struct pda_snake_state *s = &shell->snake;
    s->over = true;
    shell->tick_ms = 0;
    if (s->score > shell->scores.snake) {
        shell->scores.snake = s->score;
        shell->scores_changed = true;
    }
}

static void step(struct pda_shell *shell) {
    struct pda_snake_state *s = &shell->snake;
    s->dir = s->next_dir;
    const uint16_t head = body_at(s, 0);
    int x = head % PDA_SNAKE_W, y = head / PDA_SNAKE_W;
    switch (s->dir) {
    case PDA_DIR_UP:
        y--;
        break;
    case PDA_DIR_DOWN:
        y++;
        break;
    case PDA_DIR_LEFT:
        x--;
        break;
    default:
        x++;
        break;
    }
    const uint16_t next = (uint16_t)(y * PDA_SNAKE_W + x);
    // The tail moves away this step unless the snake grows.
    if (x < 0 || y < 0 || x >= PDA_SNAKE_W || y >= PDA_SNAKE_H || on_snake(s, next, s->grow == 0)) {
        game_over(shell);
        return;
    }
    s->head = (uint16_t)((s->head + 1) % CELLS);
    s->body[s->head] = next;
    if (next == s->food) {
        s->score++;
        s->grow += 2;
    }
    if (s->grow > 0) {
        s->grow--;
        s->len++;
    }
    if (next == s->food) {
        if (s->len >= CELLS) {
            game_over(shell);
            return;
        }
        place_food(shell);
    }
}

static void snake_open(struct pda_shell *shell) {
    const struct pda_snake_state *s = &shell->snake;
    shell->tick_ms = s->started && !s->over ? PLAY_TICK_MS : 0;
}

static bool opposite(int a, int b) {
    return (a == PDA_DIR_UP && b == PDA_DIR_DOWN) || (a == PDA_DIR_DOWN && b == PDA_DIR_UP) ||
           (a == PDA_DIR_LEFT && b == PDA_DIR_RIGHT) || (a == PDA_DIR_RIGHT && b == PDA_DIR_LEFT);
}

static enum pda_app_result snake_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                     bool pressed) {
    (void)implicit_mods;
    struct pda_snake_state *s = &shell->snake;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    if (!s->started || s->over) {
        if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE) {
            start(shell);
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    const enum pda_dir dir = pda_key_dir(usage);
    if (dir != PDA_DIR_NONE && !opposite(dir, s->dir)) {
        s->next_dir = (uint8_t)dir;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result snake_tick(struct pda_shell *shell) {
    struct pda_snake_state *s = &shell->snake;
    if (!s->started || s->over || shell->mode == ROAMYBOARD_PDA_OFF) {
        s->last_step_ms = shell->now_ms;
        return PDA_APP_NONE;
    }
    if (shell->now_ms - s->last_step_ms < step_ms(s)) {
        return PDA_APP_NONE;
    }
    s->last_step_ms = shell->now_ms;
    step(shell);
    return PDA_APP_REDRAW;
}

static void snake_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_snake_state *s = &shell->snake;
    const int fy = area.y + area.h - PDA_SNAKE_H * CELL;
    char text[24];
    snprintf(text, sizeof(text), "%lu", (unsigned long)s->score);
    pda_text_draw(fb, area.x + 1, area.y, 40, text, PDA_FONT_SMALL, true);
    snprintf(text, sizeof(text), "%lu", (unsigned long)shell->scores.snake);
    pda_text_draw(fb, area.x + area.w - 1 - pda_text_width(text, PDA_FONT_SMALL), area.y, 40, text,
                  PDA_FONT_SMALL, true);
    pda_fb_fill(fb, (struct pda_rect){area.x, fy - 2, area.w, 1}, true);

    if (s->started) {
        for (int i = 0; i < s->len; i++) {
            const uint16_t c = body_at(s, i);
            const int size = i == 0 ? CELL : CELL - 1;
            pda_fb_fill(fb, (struct pda_rect){area.x + c % PDA_SNAKE_W * CELL,
                                              fy + c / PDA_SNAKE_W * CELL, size, size},
                        true);
        }
        pda_fb_frame(fb, (struct pda_rect){area.x + s->food % PDA_SNAKE_W * CELL,
                                           fy + s->food / PDA_SNAKE_W * CELL, CELL - 1, CELL - 1},
                     true);
    }

    char best[24];
    snprintf(best, sizeof(best), "best %lu", (unsigned long)shell->scores.snake);
    if (!s->started) {
        const char *const lines[3] = {"Snake", best, "ret: play"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (s->over) {
        const char *const lines[3] = {"game over", best, "ret: again"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (shell->mode == ROAMYBOARD_PDA_OFF) {
        const char *const lines[3] = {"paused", NULL, NULL};
        pda_game_banner(fb, area, area.y + 50, lines);
    }
}

const struct pda_app pda_app_snake_def = {
    .name = "Snake",
    .open = snake_open,
    .key = snake_key,
    .tick = snake_tick,
    .draw = snake_draw,
};
