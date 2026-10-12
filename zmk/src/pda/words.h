/*
 * Word lists for the PDA's word games.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "apps.h"

/** Answers for Words, lowercase. */
extern const char pda_words_answers[][PDA_WORDS_LEN + 1];
extern const int pda_words_answer_count;

/** Falling words for Rain, lowercase, at most PDA_RAIN_WORD_MAX letters. */
extern const char *const pda_rain_words[];
extern const int pda_rain_word_count;

/**
 * Marks each letter of guess against answer, both lowercase: PDA_WORDS_CORRECT where they
 * match, then PDA_WORDS_PRESENT for a letter that answer has elsewhere, each of answer's
 * remaining letters at most once, and PDA_WORDS_ABSENT for the rest.
 */
void pda_words_mark(const char *answer, const char *guess, uint8_t marks[PDA_WORDS_LEN]);
