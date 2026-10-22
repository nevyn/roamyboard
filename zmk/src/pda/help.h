/*
 * The help screen that ? shows over any app: its keys, each with what it does. Pure C, so that
 * zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "gfx.h"

/**
 * Draws help into area, scrolled up by scroll pixels, then a last entry saying that Escape
 * closes it.
 *
 * @param fb Where to draw, or NULL to only measure.
 * @param help Entries separated by newlines. An entry "keys\tdoes" shows the keys inverted, then
 *             what they do under them; an entry without a tab is a paragraph.
 * @return The height of the whole help in pixels, to limit scrolling.
 */
int pda_help_draw(struct pda_fb *fb, struct pda_rect area, const char *help, int scroll);
