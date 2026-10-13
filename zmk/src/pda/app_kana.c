/*
 * Kana: a drill of the 46 basic hiragana and katakana. Type a kana's romaji (Hepburn, or the
 * common alternatives si, ti, tu, hu and o for wo); a kana missed comes back more often, one
 * known less. Tab switches between hiragana, katakana and both.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "keys.h"
#include "shell.h"

#define WEIGHT_START 4
#define WEIGHT_MAX 32

enum {
    USAGE_SPACE = 0x2C,
};

struct kana {
    const char *glyph;
    /** Hepburn first; then accepted alternatives, or NULL. */
    const char *romaji[2];
};

/** The 46 basic kana; katakana follow hiragana in the same order. */
static const struct kana kana[PDA_KANA_COUNT] = {
    {"あ", {"a"}},         {"い", {"i"}},         {"う", {"u"}},         {"え", {"e"}},
    {"お", {"o"}},         {"か", {"ka"}},        {"き", {"ki"}},        {"く", {"ku"}},
    {"け", {"ke"}},        {"こ", {"ko"}},        {"さ", {"sa"}},        {"し", {"shi", "si"}},
    {"す", {"su"}},        {"せ", {"se"}},        {"そ", {"so"}},        {"た", {"ta"}},
    {"ち", {"chi", "ti"}}, {"つ", {"tsu", "tu"}}, {"て", {"te"}},        {"と", {"to"}},
    {"な", {"na"}},        {"に", {"ni"}},        {"ぬ", {"nu"}},        {"ね", {"ne"}},
    {"の", {"no"}},        {"は", {"ha"}},        {"ひ", {"hi"}},        {"ふ", {"fu", "hu"}},
    {"へ", {"he"}},        {"ほ", {"ho"}},        {"ま", {"ma"}},        {"み", {"mi"}},
    {"む", {"mu"}},        {"め", {"me"}},        {"も", {"mo"}},        {"や", {"ya"}},
    {"ゆ", {"yu"}},        {"よ", {"yo"}},        {"ら", {"ra"}},        {"り", {"ri"}},
    {"る", {"ru"}},        {"れ", {"re"}},        {"ろ", {"ro"}},        {"わ", {"wa"}},
    {"を", {"wo", "o"}},   {"ん", {"n"}},

    {"ア", {"a"}},         {"イ", {"i"}},         {"ウ", {"u"}},         {"エ", {"e"}},
    {"オ", {"o"}},         {"カ", {"ka"}},        {"キ", {"ki"}},        {"ク", {"ku"}},
    {"ケ", {"ke"}},        {"コ", {"ko"}},        {"サ", {"sa"}},        {"シ", {"shi", "si"}},
    {"ス", {"su"}},        {"セ", {"se"}},        {"ソ", {"so"}},        {"タ", {"ta"}},
    {"チ", {"chi", "ti"}}, {"ツ", {"tsu", "tu"}}, {"テ", {"te"}},        {"ト", {"to"}},
    {"ナ", {"na"}},        {"ニ", {"ni"}},        {"ヌ", {"nu"}},        {"ネ", {"ne"}},
    {"ノ", {"no"}},        {"ハ", {"ha"}},        {"ヒ", {"hi"}},        {"フ", {"fu", "hu"}},
    {"ヘ", {"he"}},        {"ホ", {"ho"}},        {"マ", {"ma"}},        {"ミ", {"mi"}},
    {"ム", {"mu"}},        {"メ", {"me"}},        {"モ", {"mo"}},        {"ヤ", {"ya"}},
    {"ユ", {"yu"}},        {"ヨ", {"yo"}},        {"ラ", {"ra"}},        {"リ", {"ri"}},
    {"ル", {"ru"}},        {"レ", {"re"}},        {"ロ", {"ro"}},        {"ワ", {"wa"}},
    {"ヲ", {"wo", "o"}},   {"ン", {"n"}},
};

const char *pda_kana_glyph(int i) { return kana[i].glyph; }
const char *pda_kana_romaji(int i) { return kana[i].romaji[0]; }

static const char *const set_names[] = {"hiragana", "katakana", "both"};

/** The first and one past the last kana of the set that the drill asks. */
static void set_range(const struct pda_kana_state *k, int *first, int *end) {
    *first = k->set == PDA_KANA_KATAKANA ? PDA_KANA_COUNT / 2 : 0;
    *end = k->set == PDA_KANA_HIRAGANA ? PDA_KANA_COUNT / 2 : PDA_KANA_COUNT;
}

/** Picks the next kana by weight, never the one just asked. */
static void next(struct pda_shell *shell) {
    struct pda_kana_state *k = &shell->kana;
    int first, end;
    set_range(k, &first, &end);
    uint32_t total = 0;
    for (int i = first; i < end; i++) {
        total += i == k->current ? 0 : k->weight[i];
    }
    uint32_t pick = pda_shell_random(shell, total);
    for (int i = first; i < end; i++) {
        const uint32_t w = i == k->current ? 0 : k->weight[i];
        if (pick < w) {
            k->current = (int8_t)i;
            break;
        }
        pick -= w;
    }
    k->typed_len = 0;
    k->typed[0] = '\0';
    k->phase = PDA_KANA_ASKING;
}

static void kana_open(struct pda_shell *shell) {
    struct pda_kana_state *k = &shell->kana;
    if (!k->started) {
        for (int i = 0; i < PDA_KANA_COUNT; i++) {
            if (k->weight[i] == 0) {
                k->weight[i] = WEIGHT_START;
            }
        }
        k->started = true;
        k->current = -1;
        next(shell);
    }
}

/** Whether typed is an answer for the current kana (1), the start of one (0), or neither (-1). */
static int check(const struct pda_kana_state *k) {
    int result = -1;
    for (int a = 0; a < 2 && kana[k->current].romaji[a]; a++) {
        const char *answer = kana[k->current].romaji[a];
        if (strcmp(answer, k->typed) == 0) {
            return 1;
        }
        if (strncmp(answer, k->typed, k->typed_len) == 0) {
            result = 0;
        }
    }
    return result;
}

static enum pda_app_result kana_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed) {
    (void)implicit_mods;
    struct pda_kana_state *k = &shell->kana;
    if (!pressed) {
        return PDA_APP_NONE;
    }
    if (usage == PDA_USAGE_ESCAPE) {
        return PDA_APP_CLOSE;
    }
    if (usage == PDA_USAGE_TAB) {
        k->set = (uint8_t)((k->set + 1) % 3);
        k->current = -1;
        next(shell);
        return PDA_APP_REDRAW;
    }
    const bool letter = usage >= PDA_USAGE_A && usage < PDA_USAGE_A + 26 &&
                        !(shell->held_mods & (PDA_MODS_GUI | PDA_MODS_CTRL | PDA_MODS_ALT));
    if (k->phase != PDA_KANA_ASKING) {
        // Any key goes on to the next kana; a letter also starts its answer.
        next(shell);
        if (!letter) {
            return PDA_APP_REDRAW;
        }
    }
    if (usage == PDA_USAGE_BACKSPACE && k->typed_len > 0) {
        k->typed[--k->typed_len] = '\0';
        return PDA_APP_REDRAW;
    }
    if (!letter || k->typed_len >= sizeof(k->typed) - 1) {
        return PDA_APP_REDRAW;
    }
    k->typed[k->typed_len++] = (char)('a' + usage - PDA_USAGE_A);
    k->typed[k->typed_len] = '\0';
    const int result = check(k);
    if (result == 1) {
        k->phase = PDA_KANA_RIGHT;
        k->right++;
        k->asked++;
        if (++k->streak > k->best_streak) {
            k->best_streak = k->streak;
        }
        k->weight[k->current] = (uint8_t)(k->weight[k->current] > 1 ? k->weight[k->current] / 2 : 1);
        shell->kana_changed = true;
    } else if (result < 0) {
        k->phase = PDA_KANA_WRONG;
        k->asked++;
        k->streak = 0;
        const int w = k->weight[k->current] * 2 + 2;
        k->weight[k->current] = (uint8_t)(w > WEIGHT_MAX ? WEIGHT_MAX : w);
        shell->kana_changed = true;
    }
    return PDA_APP_REDRAW;
}

static void kana_draw(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area) {
    const struct pda_kana_state *k = &shell->kana;
    char text[24];
    pda_text_draw(fb, area.x + 1, area.y + 2, area.w, set_names[k->set], PDA_FONT_SMALL, true);
    if (k->current < 0) {
        return;
    }
    const struct pda_rect glyph_box = {area.x + 12, area.y + 14, area.w - 24, 44};
    if (k->phase == PDA_KANA_RIGHT) {
        pda_fb_fill(fb, glyph_box, true);
    }
    pda_text_center(fb, area, area.y + 18, kana[k->current].glyph, PDA_FONT_KANA,
                    k->phase != PDA_KANA_RIGHT);
    if (k->phase == PDA_KANA_WRONG) {
        pda_fb_frame(fb, glyph_box, true);
    }

    switch (k->phase) {
    case PDA_KANA_ASKING:
        snprintf(text, sizeof(text), "%s_", k->typed);
        break;
    case PDA_KANA_RIGHT:
        snprintf(text, sizeof(text), "%s", k->typed);
        break;
    case PDA_KANA_WRONG:
        snprintf(text, sizeof(text), "%s", kana[k->current].romaji[0]);
        break;
    }
    pda_text_center(fb, area, area.y + 66, text, PDA_FONT_TITLE, true);
    if (k->phase == PDA_KANA_WRONG) {
        snprintf(text, sizeof(text), "not %s", k->typed);
        pda_text_center(fb, area, area.y + 84, text, PDA_FONT_BODY, true);
    }

    const int small_h = pda_text_line_height(PDA_FONT_SMALL);
    snprintf(text, sizeof(text), "%u/%u", k->right, k->asked);
    pda_text_draw(fb, area.x + 1, area.y + area.h - 2 * small_h - 1, area.w, text, PDA_FONT_SMALL,
                  true);
    snprintf(text, sizeof(text), "run %u/%u", k->streak, k->best_streak);
    pda_text_draw(fb, area.x + 1, area.y + area.h - small_h - 1, area.w, text, PDA_FONT_SMALL,
                  true);
}

const struct pda_app pda_app_kana_def = {
    .name = "Kana",
    .open = kana_open,
    .key = kana_key,
    .draw = kana_draw,
};
