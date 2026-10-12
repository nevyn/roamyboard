/*
 * Host tests for the PDA's Timer, Calc and games, driven through the shell by key and tick.
 * Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdlib.h>

#include "pda_test_support.h"
#include "words.h"

static void new_shell(struct pda_shell *shell) {
    pda_shell_init(shell, 12345);
    shell->now_ms = 1000;
}

/* Clock */

static void test_clock_redraws_once_a_second(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Clock");
    CHECK(shell.tick_ms != 0);
    CHECK(pda_shell_tick(&shell) == PDA_SHELL_NONE); // no time yet
    shell.clock = (struct pda_clock_state){PDA_CLOCK_SET, 1, 845154330u, shell.now_ms};
    CHECK(pda_shell_tick(&shell) == PDA_SHELL_REDRAW);
    shell.now_ms += 250;
    CHECK(pda_shell_tick(&shell) == PDA_SHELL_NONE);
    shell.now_ms += 750;
    CHECK(pda_shell_tick(&shell) == PDA_SHELL_REDRAW);
}

/* Timer */

static void test_countdown_runs_out(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Timer");
    tap(&shell, USAGE_1); // 1 minute
    CHECK(shell.timer.countdown_s == 60);
    CHECK(shell.tick_ms == 0);
    tap(&shell, USAGE_SPACE);
    CHECK(shell.timer.running && shell.tick_ms != 0);
    run_ms(&shell, 30000);
    CHECK(shell.timer.running && !shell.timer.done);
    // It keeps counting while the keys go to the host.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    run_ms(&shell, 30400);
    CHECK(shell.timer.done && !shell.timer.running);
    CHECK(shell.tick_ms == 0);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    tap(&shell, USAGE_SPACE); // again
    CHECK(!shell.timer.done && shell.timer.countdown_s == 60 && shell.timer.elapsed_ms == 0);
}

static void test_stopwatch_pauses(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Timer");
    tap(&shell, USAGE_S);
    tap(&shell, USAGE_SPACE);
    run_ms(&shell, 5000);
    tap(&shell, USAGE_SPACE);
    CHECK(!shell.timer.running && shell.timer.elapsed_ms == 5000);
    run_ms(&shell, 5000);
    CHECK(shell.timer.elapsed_ms == 5000);
    tap(&shell, PDA_USAGE_BACKSPACE);
    CHECK(shell.timer.elapsed_ms == 0);
}

static void test_typed_minutes(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Timer");
    tap(&shell, USAGE_2);
    tap(&shell, USAGE_5);
    CHECK(shell.timer.countdown_s == 25 * 60);
    tap(&shell, PDA_USAGE_BACKSPACE);
    CHECK(shell.timer.countdown_s == 2 * 60);
}

/* Calc */

static void calc_type(struct pda_shell *shell, const char *keys) {
    for (const char *k = keys; *k; k++) {
        switch (*k) {
        case '0':
            tap(shell, 0x27);
            break;
        case '.':
            tap(shell, USAGE_DOT);
            break;
        case 'E':
            tap(shell, PDA_USAGE_RETURN);
            break;
        case '+':
            tap_mods(shell, LSHIFT, USAGE_EQUAL);
            break;
        case '-':
            tap(shell, USAGE_MINUS);
            break;
        case '*':
            tap_mods(shell, LSHIFT, USAGE_8);
            break;
        case '/':
            tap(shell, USAGE_SLASH);
            break;
        case 'n':
            tap(shell, USAGE_N);
            break;
        case 'r':
            tap(shell, 0x15);
            break;
        default:
            tap(shell, (uint8_t)(USAGE_1 + (*k - '1')));
            break;
        }
    }
}

static bool near(double a, double b) { return fabs(a - b) < 1e-9; }

static void test_calc_rpn(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Calc");
    calc_type(&shell, "2E3+");
    CHECK(near(shell.calc.stack[0], 5));
    calc_type(&shell, "10E4/");
    CHECK(near(shell.calc.stack[0], 2.5));
    CHECK(near(shell.calc.stack[1], 5)); // 5 stayed on the stack
    calc_type(&shell, "*");
    CHECK(near(shell.calc.stack[0], 12.5));
    calc_type(&shell, "2E3*4+");
    CHECK(near(shell.calc.stack[0], 10));
    calc_type(&shell, "5n");
    CHECK(shell.calc.entering && strcmp(shell.calc.entry, "-5") == 0);
    calc_type(&shell, "-");
    CHECK(near(shell.calc.stack[0], 15));
    calc_type(&shell, "9r");
    CHECK(near(shell.calc.stack[0], 3));
    calc_type(&shell, "1.5.5");
    CHECK(strcmp(shell.calc.entry, "1.55") == 0);
}

static void test_calc_errors_and_type_out(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Calc");
    calc_type(&shell, "1E0/");
    CHECK(shell.calc.error);
    calc_type(&shell, "1"); // clears the error only
    CHECK(!shell.calc.error && near(shell.calc.stack[0], 0));
    calc_type(&shell, "3E4/");
    CHECK(tap_mods(&shell, LGUI, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(shell.type_out_len == 4 && memcmp(shell.type_out, "0.75", 4) == 0);
}

/* Dice */

static void test_dice_parse(void) {
    int n, m, k;
    CHECK(pda_dice_parse("2d6+3", &n, &m, &k) && n == 2 && m == 6 && k == 3);
    CHECK(pda_dice_parse("d20", &n, &m, &k) && n == 1 && m == 20 && k == 0);
    CHECK(pda_dice_parse("3d8-1", &n, &m, &k) && n == 3 && m == 8 && k == -1);
    CHECK(!pda_dice_parse("0d6", &n, &m, &k));
    CHECK(!pda_dice_parse("21d6", &n, &m, &k));
    CHECK(!pda_dice_parse("d1", &n, &m, &k));
    CHECK(!pda_dice_parse("2d", &n, &m, &k));
    CHECK(!pda_dice_parse("d6+", &n, &m, &k));
    CHECK(!pda_dice_parse("2d6x", &n, &m, &k));
    CHECK(!pda_dice_parse("99999999d6", &n, &m, &k));
}

static void test_dice_rolls(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Dice");
    tap(&shell, USAGE_2);
    tap(&shell, 0x07); // d
    tap(&shell, 0x23); // 6
    tap_mods(&shell, LSHIFT, USAGE_EQUAL);
    tap(&shell, USAGE_3);
    CHECK(strcmp(shell.dice.entry, "2d6+3") == 0);
    for (int i = 0; i < 50; i++) {
        tap(&shell, PDA_USAGE_RETURN); // the first rolls the entry, the rest roll it again
        CHECK(shell.dice.total >= 5 && shell.dice.total <= 15);
        CHECK(shell.dice.roll_count == 2 && !shell.dice.invalid);
    }
    CHECK(strcmp(shell.dice.history[0], "2d6+3") == 0);
    tap(&shell, USAGE_SPACE);
    CHECK(strcmp(shell.dice.last, "d20") == 0);
    CHECK(shell.dice.total >= 1 && shell.dice.total <= 20);
}

/* Breakout */

static void test_breakout_paddle_and_serve(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Bricks");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_breakout_state *b = &shell.breakout;
    CHECK(b->started && b->serving && shell.tick_ms != 0);
    const int32_t paddle = b->paddle_q8;
    pda_shell_key(&shell, USAGE_LEFT, 0, true);
    run_ms(&shell, 200);
    pda_shell_key(&shell, USAGE_LEFT, 0, false);
    CHECK(b->paddle_q8 < paddle);
    const int32_t moved = b->paddle_q8;
    run_ms(&shell, 200);
    CHECK(b->paddle_q8 == moved);
    // The ball rides on the paddle until the serve.
    CHECK(b->ball_x_q8 > b->paddle_q8);
    tap(&shell, USAGE_SPACE);
    CHECK(!b->serving && b->vy_q8 < 0);
    run_ms(&shell, 4000);
    CHECK(b->score > 0);
}

static void test_breakout_loses_a_ball(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Bricks");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_breakout_state *b = &shell.breakout;
    tap(&shell, USAGE_SPACE);
    // A ball falling past the paddle's far side.
    b->paddle_q8 = 0;
    b->ball_x_q8 = 60 * 256;
    b->ball_y_q8 = 120 * 256;
    b->vx_q8 = 0;
    b->vy_q8 = 60 * 256;
    run_ms(&shell, 400);
    CHECK(b->lives == 2 && b->serving);
}

/* Tetris */

static void test_tetris_clears_a_line(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Tetris");
    CHECK(!shell.tetris.started);
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_tetris_state *t = &shell.tetris;
    CHECK(t->started && !t->over && shell.tick_ms != 0);
    // A bottom row with a gap that a flat I fills.
    for (int x = 0; x < PDA_TETRIS_W; x++) {
        t->board[PDA_TETRIS_H - 1][x] = (x >= 3 && x <= 6) ? 0 : 1;
    }
    t->piece = 0;
    t->rotation = 0;
    t->x = 3;
    t->y = -1;
    tap(&shell, USAGE_SPACE);
    CHECK(t->lines == 1);
    CHECK(t->score >= 100);
    for (int x = 0; x < PDA_TETRIS_W; x++) {
        CHECK(t->board[PDA_TETRIS_H - 1][x] == 0);
    }
}

static void test_tetris_gravity_waits_for_focus(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Tetris");
    tap(&shell, PDA_USAGE_RETURN);
    const int y = shell.tetris.y;
    run_ms(&shell, 850);
    CHECK(shell.tetris.y == y + 1);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    run_ms(&shell, 5000);
    CHECK(shell.tetris.y == y + 1);
}

static void test_tetris_rotates_off_the_wall(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Tetris");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_tetris_state *t = &shell.tetris;
    t->piece = 2; // T
    t->rotation = 1;
    t->y = 5;
    for (int i = 0; i < 10; i++) {
        tap(&shell, USAGE_LEFT);
    }
    const int x = t->x;
    tap(&shell, USAGE_UP);
    CHECK(t->rotation == 2);
    CHECK(t->x >= x); // kicked right, or stayed
}

static void test_tetris_ends_when_the_well_is_full(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Tetris");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_tetris_state *t = &shell.tetris;
    for (int y = 2; y < PDA_TETRIS_H; y++) {
        for (int x = 0; x < PDA_TETRIS_W - 1; x++) {
            t->board[y][x] = 1;
        }
    }
    t->piece = 1; // O
    t->rotation = 0;
    t->x = 4;
    t->y = 0;
    tap(&shell, USAGE_SPACE);
    CHECK(t->over);
    CHECK(shell.tick_ms == 0);
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(!t->over && t->score == 0);
}

/* Snake */

static uint16_t snake_head(const struct pda_snake_state *s) { return s->body[s->head]; }

static void test_snake_moves_eats_and_grows(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Snake");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_snake_state *s = &shell.snake;
    CHECK(s->started && s->len == 3);
    const uint16_t head = snake_head(s);
    s->food = (uint16_t)(head + 1);
    run_ms(&shell, 160);
    CHECK(snake_head(s) == head + 1);
    CHECK(s->score == 1);
    run_ms(&shell, 400);
    CHECK(s->len == 5);
    // Reversing into the body is ignored.
    tap(&shell, USAGE_LEFT);
    CHECK(s->next_dir == PDA_DIR_RIGHT);
    tap(&shell, USAGE_UP);
    CHECK(s->next_dir == PDA_DIR_UP);
}

static void test_snake_dies_at_the_wall(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Snake");
    tap(&shell, PDA_USAGE_RETURN);
    run_ms(&shell, 5000);
    CHECK(shell.snake.over);
    CHECK(shell.scores.snake == shell.snake.score);
}

/* Words */

static void test_words_marks_repeated_letters_once(void) {
    uint8_t m[PDA_WORDS_LEN];
    pda_words_mark("apple", "pappa", m);
    CHECK(m[0] == PDA_WORDS_PRESENT && m[1] == PDA_WORDS_PRESENT && m[2] == PDA_WORDS_CORRECT);
    CHECK(m[3] == PDA_WORDS_ABSENT && m[4] == PDA_WORDS_ABSENT);
    pda_words_mark("crane", "nacre", m);
    CHECK(m[0] == PDA_WORDS_PRESENT && m[1] == PDA_WORDS_PRESENT && m[2] == PDA_WORDS_PRESENT);
    CHECK(m[3] == PDA_WORDS_PRESENT && m[4] == PDA_WORDS_CORRECT);
}

static void test_words_game(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Words");
    struct pda_words_state *w = &shell.words;
    CHECK(w->started && strlen(w->answer) == PDA_WORDS_LEN);
    memcpy(w->answer, "crane", 6);
    type(&shell, "slat");
    tap(&shell, PDA_USAGE_RETURN); // four letters: not a guess
    CHECK(w->count == 0);
    type(&shell, "x");
    tap(&shell, PDA_USAGE_BACKSPACE);
    type(&shell, "ez"); // the z is a sixth letter, which the row has no room for
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(w->count == 1 && strcmp(w->guesses[0], "slate") == 0);
    CHECK(w->absent & (1u << ('s' - 'a')));
    CHECK(!(w->absent & (1u << ('a' - 'a'))));
    type(&shell, "crane");
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(w->over && w->won);
    CHECK(shell.scores.words_played == 1 && shell.scores.words_won == 1);
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(!w->over && w->count == 0);
}

static void test_words_answers_are_five_letters(void) {
    CHECK(pda_words_answer_count > 100);
    for (int i = 0; i < pda_words_answer_count; i++) {
        CHECK(strlen(pda_words_answers[i]) == PDA_WORDS_LEN);
    }
    for (int i = 0; i < pda_rain_word_count; i++) {
        CHECK(strlen(pda_rain_words[i]) <= PDA_RAIN_WORD_MAX);
    }
}

/* Rain */

static int active_drop(const struct pda_rain_state *r) {
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        if (r->drops[i].active) {
            return i;
        }
    }
    return -1;
}

static void test_rain_typing_clears_a_word(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Rain");
    tap(&shell, PDA_USAGE_RETURN);
    struct pda_rain_state *r = &shell.rain;
    run_ms(&shell, 40);
    const int i = active_drop(r);
    CHECK(i >= 0);
    char word[PDA_RAIN_WORD_MAX + 1];
    strcpy(word, r->drops[i].word);
    // A wrong second letter lets the word go.
    tap(&shell, (uint8_t)(USAGE_A + word[0] - 'a'));
    CHECK(r->target == i && r->drops[i].typed == 1);
    tap(&shell, (uint8_t)(USAGE_A + (word[1] == 'z' ? 0 : word[1] - 'a' + 1)));
    CHECK(r->target == -1);
    type(&shell, word);
    CHECK(!r->drops[i].active);
    CHECK(r->score == strlen(word));
}

static void test_rain_ends_after_three_misses(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Rain");
    tap(&shell, PDA_USAGE_RETURN);
    run_ms(&shell, 120000);
    CHECK(shell.rain.over && shell.rain.lives == 0);
    CHECK(shell.tick_ms == 0);
}

int main(void) {
    test_clock_redraws_once_a_second();
    test_countdown_runs_out();
    test_stopwatch_pauses();
    test_typed_minutes();
    test_calc_rpn();
    test_calc_errors_and_type_out();
    test_dice_parse();
    test_dice_rolls();
    test_breakout_paddle_and_serve();
    test_breakout_loses_a_ball();
    test_tetris_clears_a_line();
    test_tetris_gravity_waits_for_focus();
    test_tetris_rotates_off_the_wall();
    test_tetris_ends_when_the_well_is_full();
    test_snake_moves_eats_and_grows();
    test_snake_dies_at_the_wall();
    test_words_marks_repeated_letters_once();
    test_words_game();
    test_words_answers_are_five_letters();
    test_rain_typing_clears_a_word();
    test_rain_ends_after_three_misses();
    if (failures) {
        fprintf(stderr, "pda_apps_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_apps_test: all checks passed\n");
    return 0;
}
