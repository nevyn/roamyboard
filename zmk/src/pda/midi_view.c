/*
 * SPDX-License-Identifier: MIT
 */

#include "midi_view.h"

#include <stdio.h>

static const char *const note_names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                         "F#", "G",  "G#", "A",  "A#", "B"};
static const char *const velocity_names[] = {"pp", "p", "mf", "f", "ff"};
/** The white key that each note of the octave is, or lies to the right of when it is black. */
static const int8_t white_of[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};

static bool held(const struct pda_midi_view *view, int note) {
    return note >= 0 && note < 128 && (view->held[note / 8] & (1 << (note % 8)));
}

static bool is_black(int note) { return white_of[note % 12] == white_of[(note + 11) % 12]; }

int pda_midi_view_lowest(const struct pda_midi_view *view) { return 48 + 12 * view->octave; }

/** Text at twice its size, centred in area at y, or at its own size if that is too wide. */
static void draw_big(struct pda_fb *fb, struct pda_rect area, int y, const char *text) {
    static struct pda_fb scratch;
    const int w = pda_text_width(text, PDA_FONT_TITLE);
    if (2 * w > area.w) {
        pda_text_center(fb, area, y, text, PDA_FONT_TITLE, true);
        return;
    }
    const int h = pda_text_line_height(PDA_FONT_TITLE);
    pda_fb_clear(&scratch);
    pda_text_draw(&scratch, 0, 0, PDA_FB_W, text, PDA_FONT_TITLE, true);
    const int x0 = area.x + (area.w - 2 * w) / 2;
    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            if (pda_fb_get(&scratch, sx, sy)) {
                pda_fb_fill(fb, (struct pda_rect){x0 + 2 * sx, y + 2 * sy, 2, 2}, true);
            }
        }
    }
}

static void draw_octave(const struct pda_midi_view *view, struct pda_fb *fb, int y, int c) {
    const int h = PDA_MIDI_OCTAVE_H - 1;
    const int black_h = h * 3 / 5;
    const int x0 = PDA_MIDI_KEYS_X;
    pda_fb_frame(fb, (struct pda_rect){x0, y, 7 * PDA_MIDI_WHITE_W + 1, h}, true);
    for (int i = 1; i < 7; i++) {
        pda_fb_fill(fb, (struct pda_rect){x0 + i * PDA_MIDI_WHITE_W, y, 1, h}, true);
    }
    for (int n = 0; n < 12; n++) {
        if (!is_black(c + n) && held(view, c + n)) {
            // Only below the black keys, so that it never touches one.
            pda_fb_fill(fb, (struct pda_rect){x0 + white_of[n] * PDA_MIDI_WHITE_W + 2,
                                              y + black_h + 2, PDA_MIDI_WHITE_W - 3,
                                              h - black_h - 4},
                        true);
        }
    }
    for (int n = 0; n < 12; n++) {
        if (is_black(c + n)) {
            const struct pda_rect key = {x0 + (white_of[n] + 1) * PDA_MIDI_WHITE_W - 2, y, 5,
                                         black_h};
            // A held black key shows hollow, the opposite of a held white key.
            pda_fb_fill(fb, key, !held(view, c + n));
            pda_fb_frame(fb, key, true);
        }
    }
}

void pda_midi_view_draw(const struct pda_midi_view *view, struct pda_fb *fb,
                        struct pda_rect area) {
    char text[24];
    if (view->last_note >= 0) {
        snprintf(text, sizeof(text), "%s%d", note_names[view->last_note % 12],
                 view->last_note / 12 - 1);
    } else {
        snprintf(text, sizeof(text), "--");
    }
    draw_big(fb, area, area.y + 4, text);

    const int line_h = pda_text_line_height(PDA_FONT_BODY);
    snprintf(text, sizeof(text), "Oct%+d Tr%+d", view->octave, view->transpose);
    pda_text_center(fb, area, area.y + 38, text, PDA_FONT_BODY, true);
    snprintf(text, sizeof(text), "%s  Ch %d",
             velocity_names[view->velocity_step < 5 ? view->velocity_step : 2],
             view->channel + 1);
    pda_text_center(fb, area, area.y + 38 + line_h, text, PDA_FONT_BODY, true);

    const int lowest = pda_midi_view_lowest(view);
    for (int i = 0; i < 3; i++) {
        draw_octave(view, fb, PDA_MIDI_KEYS_Y + i * PDA_MIDI_OCTAVE_H, lowest + 12 * (2 - i));
    }
}
