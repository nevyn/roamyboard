/*
 * Rain: words fall; type one to clear it. The first letter picks the lowest word that starts
 * with it; a wrong letter lets the word go again. A word that reaches the bottom costs one of
 * three lives. Words fall faster and come more often as the score grows.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"
#include "words.h"

#define PLAY_TICK_MS 40
/** A longer gap between ticks is a pause, which does not count as time played. */
#define MAX_STEP_MS 200
#define LIVES 3
#define STATUS_H 10

enum {
    USAGE_SPACE = 0x2C,
};

static int play_height(struct pda_rect area) { return area.h - STATUS_H; }

/** Fall speed in pixels per second. */
static uint32_t speed(const struct pda_rain_state *r) {
    const uint32_t v = 6 + r->score / 8;
    return v > 30 ? 30 : v;
}

static uint32_t drop_interval_ms(const struct pda_rain_state *r) {
    const int ms = 2600 - 25 * (int)r->score;
    return ms < 900 ? 900 : (uint32_t)ms;
}

static void start(struct pda_shell *shell) {
    struct pda_rain_state *r = &shell->rain;
    memset(r, 0, sizeof(*r));
    r->started = true;
    r->lives = LIVES;
    r->target = -1;
    r->last_ms = shell->now_ms;
    r->next_drop_ms = shell->now_ms;
    shell->tick_ms = PLAY_TICK_MS;
}

static void spawn(struct pda_shell *shell) {
    struct pda_rain_state *r = &shell->rain;
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        struct pda_rain_drop *d = &r->drops[i];
        if (d->active) {
            continue;
        }
        const char *word = pda_rain_words[pda_shell_random(shell, (uint32_t)pda_rain_word_count)];
        memset(d, 0, sizeof(*d));
        d->len = (uint8_t)strlen(word);
        memcpy(d->word, word, d->len);
        const int width = pda_text_width(d->word, PDA_FONT_SMALL);
        const int room = PDA_FB_W - 2 - width;
        d->x = (int16_t)(1 + (room > 0 ? (int)pda_shell_random(shell, (uint32_t)room + 1) : 0));
        d->active = true;
        return;
    }
}

static void lose_drop(struct pda_shell *shell, int i) {
    struct pda_rain_state *r = &shell->rain;
    r->drops[i].active = false;
    if (r->target == i) {
        r->target = -1;
    }
    if (--r->lives == 0) {
        r->over = true;
        shell->tick_ms = 0;
        if (r->score > shell->scores.rain) {
            shell->scores.rain = r->score;
            shell->scores_changed = true;
        }
    }
}

/** Advances the rain to now: words fall, a new one comes when it is due. */
static void advance(struct pda_shell *shell, int height) {
    struct pda_rain_state *r = &shell->rain;
    uint32_t dt = shell->now_ms - r->last_ms;
    r->last_ms = shell->now_ms;
    if (dt > MAX_STEP_MS) {
        // Back from a pause: the rain goes on where it stopped, and the next word is as due as
        // it was.
        r->next_drop_ms += dt - PLAY_TICK_MS;
        dt = PLAY_TICK_MS;
    }
    for (int i = 0; i < PDA_RAIN_DROPS && !r->over; i++) {
        struct pda_rain_drop *d = &r->drops[i];
        if (!d->active) {
            continue;
        }
        d->y_q8 += (int32_t)(speed(r) * dt * 256 / 1000);
        if ((d->y_q8 >> 8) + pda_text_line_height(PDA_FONT_SMALL) > height) {
            lose_drop(shell, i);
        }
    }
    if (!r->over && (int32_t)(shell->now_ms - r->next_drop_ms) >= 0) {
        spawn(shell);
        r->next_drop_ms = shell->now_ms + drop_interval_ms(r);
    }
}

static void type_letter(struct pda_rain_state *r, char c) {
    if (r->target < 0) {
        int lowest = -1;
        for (int i = 0; i < PDA_RAIN_DROPS; i++) {
            const struct pda_rain_drop *d = &r->drops[i];
            if (d->active && d->word[0] == c &&
                (lowest < 0 || d->y_q8 > r->drops[lowest].y_q8)) {
                lowest = i;
            }
        }
        if (lowest < 0) {
            return;
        }
        r->target = (int8_t)lowest;
        r->drops[lowest].typed = 0;
    }
    struct pda_rain_drop *d = &r->drops[r->target];
    if (d->word[d->typed] != c) {
        d->typed = 0;
        r->target = -1;
        return;
    }
    if (++d->typed == d->len) {
        r->score += d->len;
        d->active = false;
        r->target = -1;
    }
}

static void rain_open(struct pda_shell *shell) {
    struct pda_rain_state *r = &shell->rain;
    shell->tick_ms = r->started && !r->over ? PLAY_TICK_MS : 0;
    // The time closed does not count: the next word is as due as it was.
    r->next_drop_ms += shell->now_ms - r->last_ms;
    r->last_ms = shell->now_ms;
}

static enum pda_app_result rain_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    (void)implicit_mods;
    struct pda_rain_state *r = &shell->rain;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    if (!r->started || r->over) {
        if (usage == PDA_USAGE_RETURN || usage == PDA_USAGE_KP_ENTER || usage == USAGE_SPACE) {
            start(shell);
            return PDA_APP_REDRAW;
        }
        return PDA_APP_NONE;
    }
    if (usage >= PDA_USAGE_A && usage < PDA_USAGE_A + 26) {
        type_letter(r, (char)('a' + usage - PDA_USAGE_A));
        return PDA_APP_REDRAW;
    }
    return PDA_APP_NONE;
}

static enum pda_app_result rain_tick(struct pda_shell *shell) {
    struct pda_rain_state *r = &shell->rain;
    if (!r->started || r->over) {
        return PDA_APP_NONE;
    }
    // Redraws only when a word moved by a whole pixel, came or went.
    int32_t before[PDA_RAIN_DROPS];
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        before[i] = r->drops[i].active ? r->drops[i].y_q8 >> 8 : -1;
    }
    const uint8_t lives = r->lives;
    advance(shell, play_height((struct pda_rect){0, PDA_TITLE_H, PDA_FB_W,
                                                 PDA_FB_H - PDA_TITLE_H}));
    bool moved = lives != r->lives || r->over;
    for (int i = 0; i < PDA_RAIN_DROPS; i++) {
        moved = moved || before[i] != (r->drops[i].active ? r->drops[i].y_q8 >> 8 : -1);
    }
    return moved ? PDA_APP_REDRAW : PDA_APP_NONE;
}

static void rain_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_rain_state *r = &shell->rain;
    char text[24];
    snprintf(text, sizeof(text), "%lu", (unsigned long)r->score);
    pda_text_draw(fb, area.x + 1, area.y, 40, text, PDA_FONT_SMALL, true);
    for (int i = 0; i < LIVES; i++) {
        const struct pda_rect life = {area.x + area.w - 6 * (LIVES - i), area.y + 2, 4, 4};
        if (i < r->lives) {
            pda_fb_fill(fb, life, true);
        } else {
            pda_fb_frame(fb, life, true);
        }
    }
    const int top = area.y + STATUS_H;
    pda_fb_fill(fb, (struct pda_rect){area.x, top - 1, area.w, 1}, true);
    pda_fb_fill(fb, (struct pda_rect){area.x, area.y + area.h - 1, area.w, 1}, true);

    const int glyph_w = pda_text_width("m", PDA_FONT_SMALL);
    const int line_h = pda_text_line_height(PDA_FONT_SMALL);
    for (int i = 0; i < PDA_RAIN_DROPS && r->started; i++) {
        const struct pda_rain_drop *d = &r->drops[i];
        if (!d->active) {
            continue;
        }
        const int y = top + (d->y_q8 >> 8);
        pda_text_draw(fb, d->x, y, area.w - d->x, d->word, PDA_FONT_SMALL, true);
        if (i == r->target && d->typed > 0) {
            pda_fb_invert(fb, (struct pda_rect){d->x, y, d->typed * glyph_w, line_h});
        }
    }

    char best[24];
    snprintf(best, sizeof(best), "best %lu", (unsigned long)shell->scores.rain);
    if (!r->started) {
        const char *const lines[3] = {"Rain", best, "ret: play"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (r->over) {
        const char *const lines[3] = {"game over", best, "ret: again"};
        pda_game_banner(fb, area, area.y + 40, lines);
    } else if (shell->mode == ROAMYBOARD_PDA_OFF) {
        const char *const lines[3] = {"paused", NULL, NULL};
        pda_game_banner(fb, area, area.y + 50, lines);
    }
}

const struct pda_app pda_app_rain_def = {
    .name = "Rain",
    .open = rain_open,
    .key = rain_key,
    .tick = rain_tick,
    .focused_ticks = true,
    .draw = rain_draw,
};
