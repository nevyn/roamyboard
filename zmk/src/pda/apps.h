/*
 * The PDA apps' state, which struct pda_shell holds, one member per app. docs/pda.md describes
 * the apps.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Keys: which key positions are held now and which have been pressed. */
struct pda_keys_state {
    uint8_t held[16];
    uint8_t seen[16];
    /** The latest pressed position and HID usage, or -1. */
    int last_position;
    int last_usage;
};

/** Clock: the second that the view shows, to redraw only when it changes. */
struct pda_clock_app_state {
    uint32_t shown_s;
};

/** Timer: a stopwatch, or a countdown when countdown_s is not 0. */
struct pda_timer_state {
    bool running;
    /** The countdown reached zero. */
    bool done;
    uint32_t countdown_s;
    /** Milliseconds counted before the current run. */
    uint32_t elapsed_ms;
    /** Uptime when the current run started. */
    uint32_t started_ms;
    /** Minutes typed for the next countdown, or 0. */
    uint16_t entry;
    /** The second that the view shows, to redraw only when it changes. */
    uint32_t shown_s;
};

/** Calc: an RPN calculator with a four-level stack; stack[0] is X. */
struct pda_calc_state {
    double stack[4];
    /** The number being typed into X. */
    char entry[16];
    uint8_t entry_len;
    bool entering;
    /** The next number typed pushes the stack, as after an operation. */
    bool lift;
    bool error;
};

#define PDA_DICE_ENTRY 12
#define PDA_DICE_ROLLS 20
#define PDA_DICE_HISTORY 3

/** Dice: an expression NdM+K typed, and the latest rolls. */
struct pda_dice_state {
    char entry[PDA_DICE_ENTRY + 1];
    uint8_t entry_len;
    /** The latest expression rolled, for Return on an empty line. */
    char last[PDA_DICE_ENTRY + 1];
    int total;
    bool rolled;
    /** The expression could not be read. */
    bool invalid;
    uint16_t rolls[PDA_DICE_ROLLS];
    uint8_t roll_count;
    int16_t bonus;
    /** Earlier results, newest first: the expression and its total. */
    char history[PDA_DICE_HISTORY][PDA_DICE_ENTRY + 1];
    int history_total[PDA_DICE_HISTORY];
};

#define PDA_TETRIS_W 10
#define PDA_TETRIS_H 20

struct pda_tetris_state {
    /** 0 for an empty cell, otherwise the piece that left it, plus one. */
    uint8_t board[PDA_TETRIS_H][PDA_TETRIS_W];
    uint8_t piece, rotation, next;
    int8_t x, y;
    /** The 7-bag: the pieces left to deal before the next shuffle. */
    uint8_t bag[7];
    uint8_t bag_left;
    uint32_t score, lines;
    uint8_t level;
    bool started, over, paused;
    uint32_t last_fall_ms;
};

#define PDA_SNAKE_W 17
#define PDA_SNAKE_H 32

struct pda_snake_state {
    /** Cells as y * PDA_SNAKE_W + x, a ring with the head at head. */
    uint16_t body[PDA_SNAKE_W * PDA_SNAKE_H];
    uint16_t head, len;
    uint16_t food;
    /** enum pda_dir: the direction moved last, and the one to move next. */
    uint8_t dir, next_dir;
    /** Cells left to grow. */
    uint8_t grow;
    uint32_t score;
    uint32_t last_step_ms;
    bool started, over;
};

#define PDA_WORDS_LEN 5
#define PDA_WORDS_TRIES 6

enum pda_words_mark {
    PDA_WORDS_ABSENT,
    PDA_WORDS_PRESENT,
    PDA_WORDS_CORRECT,
};

struct pda_words_state {
    char answer[PDA_WORDS_LEN + 1];
    char guesses[PDA_WORDS_TRIES][PDA_WORDS_LEN + 1];
    /** enum pda_words_mark per letter of each guess. */
    uint8_t marks[PDA_WORDS_TRIES][PDA_WORDS_LEN];
    uint8_t count;
    char entry[PDA_WORDS_LEN + 1];
    uint8_t entry_len;
    /** Bit n: letter 'a' + n is in no guess's answer. */
    uint32_t absent;
    bool started, over, won;
};

#define PDA_BREAKOUT_COLS 6
#define PDA_BREAKOUT_ROWS 6

struct pda_breakout_state {
    /** Bit c of row r: the brick in column c is still there. */
    uint8_t bricks[PDA_BREAKOUT_ROWS];
    /** Paddle's left edge and ball's top left, in pixels times 256, from the field's corner. */
    int32_t paddle_q8;
    int32_t ball_x_q8, ball_y_q8;
    /** Ball velocity in pixels per second times 256. */
    int32_t vx_q8, vy_q8;
    /** The direction keys held. */
    bool left_held, right_held;
    /** The ball sits on the paddle until Space. */
    bool serving;
    uint8_t lives, level;
    uint32_t score;
    uint32_t last_ms;
    bool started, over;
};

#define PDA_RAIN_DROPS 4
#define PDA_RAIN_WORD_MAX 8

struct pda_rain_drop {
    char word[PDA_RAIN_WORD_MAX + 1];
    uint8_t len;
    /** Letters typed so far, while this drop is the target. */
    uint8_t typed;
    int16_t x;
    /** Top of the word in pixels below the play area's top, times 256. */
    int32_t y_q8;
    bool active;
};

struct pda_rain_state {
    struct pda_rain_drop drops[PDA_RAIN_DROPS];
    /** The drop being typed, or -1. */
    int8_t target;
    uint32_t score;
    uint8_t lives;
    uint32_t last_ms;
    uint32_t next_drop_ms;
    bool started, over;
};

struct pda_app;
extern const struct pda_app pda_app_note_def;
extern const struct pda_app pda_app_clock_def;
extern const struct pda_app pda_app_timer_def;
extern const struct pda_app pda_app_calc_def;
extern const struct pda_app pda_app_dice_def;
extern const struct pda_app pda_app_tetris_def;
extern const struct pda_app pda_app_snake_def;
extern const struct pda_app pda_app_words_def;
extern const struct pda_app pda_app_rain_def;
extern const struct pda_app pda_app_breakout_def;
extern const struct pda_app pda_app_keys_def;
extern const struct pda_app pda_app_info_def;

/**
 * Parses a Dice roll: NdM, NdM+K or NdM-K, where N may be left out for 1. N is 1 to 20, M 2 to
 * 1000, K 0 to 999.
 */
bool pda_dice_parse(const char *text, int *count, int *sides, int *bonus);
