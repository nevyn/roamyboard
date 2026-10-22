/*
 * Host tests for the PDA's Timer, Calc and games, driven through the shell by key and tick.
 * Run with zmk/tests/run.sh.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdlib.h>

#include "pda_test_support.h"
#include "cat_sprites.h"
#include "words.h"

static int active_drop_index(const struct pda_rain_state *r) {
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        if (r->drops[i].active) {
            return i;
        }
    }
    return -1;
}

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
    CHECK(near(shell.calc.stack[0], 0) && near(shell.calc.stack[1], 1)); // nothing consumed
    calc_type(&shell, "2"); // clears the error and counts
    CHECK(!shell.calc.error && strcmp(shell.calc.entry, "2") == 0);
    calc_type(&shell, "/");
    CHECK(near(shell.calc.stack[0], 0.5));
    calc_type(&shell, "4nr"); // the square root of -4
    CHECK(shell.calc.error && near(shell.calc.stack[0], -4));
    shell.calc.stack[0] = shell.calc.stack[1] = 1e300;
    calc_type(&shell, "*"); // overflows
    CHECK(shell.calc.error && near(shell.calc.stack[0] / 1e300, 1));
    calc_type(&shell, "3E4/");
    CHECK(tap_mods(&shell, LGUI, PDA_USAGE_RETURN) == PDA_SHELL_TYPE_OUT);
    CHECK(shell.type_out_len == 4 && memcmp(shell.type_out, "0.75", 4) == 0);
}

/* Ticks */

static void test_games_do_not_tick_without_focus(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Snake");
    tap(&shell, PDA_USAGE_RETURN);
    CHECK(pda_shell_tick_ms(&shell) != 0);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(pda_shell_tick_ms(&shell) == 0);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    CHECK(pda_shell_tick_ms(&shell) != 0);

    open_named(&shell, "Timer");
    tap(&shell, USAGE_SPACE);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(pda_shell_tick_ms(&shell) != 0); // the timer counts on
}

static void test_rain_does_not_fall_during_a_pause(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Rain");
    tap(&shell, PDA_USAGE_RETURN);
    run_ms(&shell, 400);
    const int i = active_drop_index(&shell.rain);
    CHECK(i >= 0);
    const int32_t y = shell.rain.drops[i].y_q8;
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    shell.now_ms += 60000;
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_HOME);
    pda_shell_tick(&shell);
    CHECK(shell.rain.drops[i].y_q8 - y < 2 * 256);
    CHECK(shell.rain.lives == 3);
}

/* Kana */

static void test_kana_table(void) {
    for (int i = 0; i < PDA_KANA_COUNT; i++) {
        const unsigned char *g = (const unsigned char *)pda_kana_glyph(i);
        CHECK(strlen((const char *)g) == 3 && g[0] == 0xE3); // one kana, U+3040 to U+30FF
        for (const char *r = pda_kana_romaji(i); *r; r++) {
            CHECK(*r >= 'a' && *r <= 'z');
        }
    }
    // Katakana mirror hiragana's order.
    for (int i = 0; i < PDA_KANA_COUNT / 2; i++) {
        CHECK(strcmp(pda_kana_romaji(i), pda_kana_romaji(i + PDA_KANA_COUNT / 2)) == 0);
    }
}

static int kana_index(const char *glyph) {
    for (int i = 0; i < PDA_KANA_COUNT; i++) {
        if (strcmp(pda_kana_glyph(i), glyph) == 0) {
            return i;
        }
    }
    return -1;
}

static void test_kana_drill(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_named(&shell, "Kana");
    struct pda_kana_state *k = &shell.kana;
    CHECK(k->current >= 0 && k->current < PDA_KANA_COUNT / 2); // hiragana first

    const int shi = kana_index("し");
    k->current = (int8_t)shi;
    type(&shell, "si");
    CHECK(k->phase == PDA_KANA_RIGHT && k->right == 1 && k->weight[shi] == 2);

    tap(&shell, PDA_USAGE_RETURN); // next
    CHECK(k->phase == PDA_KANA_ASKING && k->current != shi);
    k->current = (int8_t)shi;
    type(&shell, "sa");
    CHECK(k->phase == PDA_KANA_WRONG && k->streak == 0 && k->weight[shi] == 6);
    CHECK(shell.kana_changed);

    // A letter after a result starts the next kana's answer.
    type(&shell, "k");
    CHECK(k->current != shi && strcmp(k->typed, "k") == 0);

    tap(&shell, PDA_USAGE_TAB);
    CHECK(k->set == PDA_KANA_KATAKANA && k->current >= PDA_KANA_COUNT / 2);
    for (int i = 0; i < 200; i++) {
        tap(&shell, PDA_USAGE_RETURN);
        tap(&shell, USAGE_A); // a right or wrong answer, then on
        CHECK(k->current >= PDA_KANA_COUNT / 2);
    }
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
    tap(&shell, USAGE_SPACE);
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

/* Cat */

enum { USAGE_F = 0x09 };

static int cat_x(const struct pda_shell *shell) { return shell->cat.x16 / 16; }

static void open_cat(struct pda_shell *shell) {
    open_named(shell, "Cat");
    CHECK(strcmp(app_name(shell), "Cat") == 0);
}

static void test_cat_comes_over_and_goes_home(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    CHECK(!pda_shell_cat_here(&shell));
    open_cat(&shell);
    CHECK(pda_shell_cat_here(&shell));
    CHECK(cat_x(&shell) < 0); // still on the left half
    run_ms(&shell, 400);
    CHECK(cat_x(&shell) < 0);
    run_ms(&shell, 2000);
    CHECK(cat_x(&shell) >= 0 && shell.cat.level == 0);

    // Escape sends her running off the left edge; then the app closes.
    CHECK(tap(&shell, PDA_USAGE_ESCAPE) != PDA_SHELL_LEAVE);
    CHECK(strcmp(app_name(&shell), "Cat") == 0);
    run_ms(&shell, 3000);
    CHECK(strcmp(app_name(&shell), "home") == 0);
    CHECK(!shell.cat.here && !pda_shell_cat_here(&shell));
}

static void test_cat_second_escape_closes_at_once(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_cat(&shell);
    run_ms(&shell, 2000);
    tap(&shell, PDA_USAGE_ESCAPE);
    CHECK(strcmp(app_name(&shell), "Cat") == 0);
    tap(&shell, PDA_USAGE_ESCAPE);
    CHECK(strcmp(app_name(&shell), "home") == 0 && !shell.cat.here);
}

static void test_cat_lives_unfocused(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_cat(&shell);
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(pda_shell_tick_ms(&shell) != 0);
    int moves = 0, last_x = cat_x(&shell), levels_seen = 0;
    for (int minute = 0; minute < 60; minute++) {
        run_ms(&shell, 60000);
        moves += cat_x(&shell) != last_x;
        last_x = cat_x(&shell);
        levels_seen |= 1 << shell.cat.level;
        CHECK(cat_x(&shell) >= 0 && cat_x(&shell) <= PDA_FB_W - CAT_WIDTH);
    }
    CHECK(moves > 10);
    CHECK(levels_seen == 7);
    CHECK(shell.cat.needs.food < 80 && shell.cat.needs.fun < 80);
    CHECK(strcmp(app_name(&shell), "Cat") == 0 && shell.cat.here);
}

static void test_cat_eats_from_the_bowl(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    shell.cat.needs.food = 20;
    open_cat(&shell);
    run_ms(&shell, 20000); // she waits at the empty bowl
    CHECK(shell.cat.level == 0 && shell.cat.needs.bowl == 0);
    shell.cat_changed = false;
    tap(&shell, USAGE_F);
    CHECK(shell.cat.needs.bowl == PDA_CAT_BOWL_MAX && shell.cat_changed);
    run_ms(&shell, 40000);
    CHECK(shell.cat.needs.food > 50);
    CHECK(shell.cat.needs.bowl < PDA_CAT_BOWL_MAX);
}

static void test_cat_sleeps_in_bed(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    shell.cat.needs.energy = 10;
    open_cat(&shell);
    run_ms(&shell, 20000);
    CHECK(shell.cat.level == 2 && cat_x(&shell) == 3);
    const uint8_t energy = shell.cat.needs.energy;
    // Asleep, she wakes the platform only a few times a second.
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(pda_shell_tick_ms(&shell) >= 250);
    run_ms(&shell, 120000);
    CHECK(shell.cat.needs.energy > energy + 10);
    CHECK(shell.cat.level == 2);
}

static void test_cat_plays_with_the_ball(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    shell.cat.needs.fun = 30;
    open_cat(&shell);
    run_ms(&shell, 2000);
    tap(&shell, USAGE_SPACE);
    CHECK(shell.cat.ball);
    run_ms(&shell, 20000);
    CHECK(shell.cat.needs.fun > 30);
    CHECK(shell.cat.ball);
}

static void test_cat_chases_the_laser(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    shell.cat.needs.fun = 30;
    open_cat(&shell);
    run_ms(&shell, 2000);
    tap(&shell, USAGE_RIGHT); // the dot appears mid-floor
    CHECK(shell.cat.laser);
    for (int i = 0; i < 6; i++) {
        tap(&shell, USAGE_RIGHT);
    }
    run_ms(&shell, 4000);
    const int paw = shell.cat.left ? cat_x(&shell) + 1 : cat_x(&shell) + CAT_WIDTH - 2;
    CHECK(abs(paw - shell.cat.laser_x) <= 3);
    CHECK(shell.cat.needs.fun > 30);
    run_ms(&shell, 7000);
    CHECK(!shell.cat.laser);
}

static void test_cat_follows_the_laser_up_the_tree(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_cat(&shell);
    run_ms(&shell, 2000);
    tap(&shell, USAGE_UP);
    for (int i = 0; i < 20; i++) { // up onto the top shelf, on the left
        tap(&shell, USAGE_UP);
        if (i < 6) {
            tap(&shell, USAGE_LEFT);
        }
    }
    CHECK(shell.cat.laser_y < 54);
    for (int i = 0; i < 6 && shell.cat.level != 2; i++) {
        run_ms(&shell, 1000);
        tap(&shell, USAGE_LEFT); // keeps the dot alive
        tap(&shell, USAGE_RIGHT);
    }
    CHECK(shell.cat.level == 2);
}

static void test_cat_purrs_when_petted(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_cat(&shell);
    run_ms(&shell, 2000);
    const uint8_t fun = shell.cat.needs.fun;
    CHECK(tap(&shell, PDA_USAGE_RETURN) == PDA_SHELL_REDRAW);
    CHECK(shell.cat.needs.fun == fun + 5);
    CHECK(shell.tick_ms == 50); // the heart rises
}

static void test_cat_stays_through_quick_note(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    open_cat(&shell);
    run_ms(&shell, 5000);
    const int32_t x = shell.cat.x16;
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_NOTE);
    CHECK(pda_shell_cat_here(&shell));
    pda_shell_set_mode(&shell, ROAMYBOARD_PDA_OFF);
    CHECK(strcmp(app_name(&shell), "Cat") == 0);
    CHECK(shell.cat.here && shell.cat.x16 == x);
}

static void test_cat_redraws_only_on_change(void) {
    static struct pda_shell shell;
    new_shell(&shell);
    shell.cat.needs.energy = 10;
    open_cat(&shell);
    run_ms(&shell, 20000); // asleep in bed
    int redraws = 0;
    for (int i = 0; i < 40; i++) {
        shell.now_ms += pda_shell_tick_ms(&shell);
        redraws += pda_shell_tick(&shell) == PDA_SHELL_REDRAW;
    }
    // Ten seconds of 250 ms ticks: a snore frame a second, and perhaps a meter.
    CHECK(redraws >= 9 && redraws <= 12);
}

int main(void) {
    test_clock_redraws_once_a_second();
    test_countdown_runs_out();
    test_stopwatch_pauses();
    test_typed_minutes();
    test_calc_rpn();
    test_calc_errors_and_type_out();
    test_games_do_not_tick_without_focus();
    test_rain_does_not_fall_during_a_pause();
    test_kana_table();
    test_kana_drill();
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
    test_cat_comes_over_and_goes_home();
    test_cat_second_escape_closes_at_once();
    test_cat_lives_unfocused();
    test_cat_eats_from_the_bowl();
    test_cat_sleeps_in_bed();
    test_cat_plays_with_the_ball();
    test_cat_chases_the_laser();
    test_cat_follows_the_laser_up_the_tree();
    test_cat_purrs_when_petted();
    test_cat_stays_through_quick_note();
    test_cat_redraws_only_on_change();
    if (failures) {
        fprintf(stderr, "pda_apps_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("pda_apps_test: all checks passed\n");
    return 0;
}
