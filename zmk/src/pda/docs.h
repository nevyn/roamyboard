/*
 * The Note app's documents: up to PDA_DOCS_MAX notes in fixed slots, each with the stamps that
 * order them, and the record of which slots differ from settings. Pure C, so that zmk/tests can
 * run it on the host. docs/pda.md describes the Note app.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "note.h"

#define PDA_DOCS_MAX 16

/** How the list orders the documents. */
enum pda_docs_sort {
    /** Newest first. */
    PDA_DOCS_BY_CREATED,
    /** Last edited first. */
    PDA_DOCS_BY_MODIFIED,
    /** By title, A to Z, ignoring ASCII case. */
    PDA_DOCS_BY_TITLE,
    PDA_DOCS_SORT_COUNT,
};

struct pda_doc {
    bool used;
    /**
     * Stamps from pda_docs.next_stamp, which only grows: a later stamp is a later moment. The
     * keyboard may have no clock, so the stamps order events instead of dating them.
     */
    uint32_t created, modified;
    /** The text, and the editor's state while the document is open. */
    struct pda_note note;
};

struct pda_docs {
    struct pda_doc docs[PDA_DOCS_MAX];
    uint32_t next_stamp;
    uint8_t sort;
    /** Bit n: slot n's text or stamps differ from settings. */
    uint16_t dirty;
    /** Bit n: settings still hold slot n, which has been deleted. */
    uint16_t gone;
    /** The sort differs from settings. */
    bool sort_changed;
};

void pda_docs_init(struct pda_docs *docs);

/** Number of documents. */
int pda_docs_count(const struct pda_docs *docs);

/**
 * Makes an empty document, created and modified now.
 * @return Its slot, or -1 when all PDA_DOCS_MAX slots are in use.
 */
int pda_docs_create(struct pda_docs *docs);

/** Deletes the document in slot; settings lose it at the next save. */
void pda_docs_delete(struct pda_docs *docs, int slot);

/** Marks slot's document as modified now, after its text changed. */
void pda_docs_touch(struct pda_docs *docs, int slot);

/**
 * Puts a document from settings into slot, as settings have it: not dirty. Later stamps continue
 * after the highest loaded.
 */
void pda_docs_load(struct pda_docs *docs, int slot, uint32_t created, uint32_t modified,
                   const char *text, size_t len);

/**
 * Writes slot's title into out: its first line, without leading and trailing spaces, cut at a
 * whole UTF-8 sequence to fit size; "" when the first line is empty.
 */
void pda_docs_title(const struct pda_docs *docs, int slot, char *out, size_t size);

/**
 * Fills order with the used slots in the order of docs->sort.
 * @return The number of slots written.
 */
int pda_docs_order(const struct pda_docs *docs, int8_t order[PDA_DOCS_MAX]);

/** Resets every document's held modifiers and dead key, as when focus changes. */
void pda_docs_reset_keys(struct pda_docs *docs);
