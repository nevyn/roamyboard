/*
 * SPDX-License-Identifier: MIT
 */

#include "docs.h"

#include <string.h>

void pda_docs_init(struct pda_docs *docs) {
    memset(docs, 0, sizeof(*docs));
    docs->next_stamp = 1;
}

int pda_docs_count(const struct pda_docs *docs) {
    int n = 0;
    for (int i = 0; i < PDA_DOCS_MAX; i++) {
        n += docs->docs[i].used;
    }
    return n;
}

int pda_docs_create(struct pda_docs *docs) {
    for (int i = 0; i < PDA_DOCS_MAX; i++) {
        struct pda_doc *doc = &docs->docs[i];
        if (!doc->used) {
            doc->used = true;
            doc->created = doc->modified = docs->next_stamp++;
            pda_note_init(&doc->note);
            docs->dirty |= (uint16_t)(1u << i);
            docs->gone &= (uint16_t)~(1u << i);
            return i;
        }
    }
    return -1;
}

void pda_docs_delete(struct pda_docs *docs, int slot) {
    struct pda_doc *doc = &docs->docs[slot];
    if (!doc->used) {
        return;
    }
    doc->used = false;
    pda_note_init(&doc->note);
    docs->dirty &= (uint16_t)~(1u << slot);
    docs->gone |= (uint16_t)(1u << slot);
}

void pda_docs_touch(struct pda_docs *docs, int slot) {
    docs->docs[slot].modified = docs->next_stamp++;
    docs->dirty |= (uint16_t)(1u << slot);
}

void pda_docs_load(struct pda_docs *docs, int slot, uint32_t created, uint32_t modified,
                   const char *text, size_t len) {
    struct pda_doc *doc = &docs->docs[slot];
    doc->used = true;
    doc->created = created;
    doc->modified = modified;
    pda_note_init(&doc->note);
    pda_note_set_text(&doc->note, text, len);
    const uint32_t last = created > modified ? created : modified;
    if (docs->next_stamp <= last) {
        docs->next_stamp = last + 1;
    }
}

/** The title's start and length in slot's text: its first line, without outer spaces. */
static const char *title_span(const struct pda_docs *docs, int slot, size_t *len) {
    const char *text = docs->docs[slot].note.text;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    size_t n = 0;
    while (text[n] != '\0' && text[n] != '\n') {
        n++;
    }
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) {
        n--;
    }
    *len = n;
    return text;
}

void pda_docs_title(const struct pda_docs *docs, int slot, char *out, size_t size) {
    size_t len;
    const char *text = title_span(docs, slot, &len);
    if (len > size - 1) {
        len = size - 1;
        while (len > 0 && ((unsigned char)text[len] & 0xC0) == 0x80) {
            len--; // never cut inside a UTF-8 sequence
        }
    }
    memcpy(out, text, len);
    out[len] = '\0';
}

static int fold(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }

/** < 0 when slot a comes before slot b in the list. */
static int compare(const struct pda_docs *docs, int a, int b) {
    const struct pda_doc *da = &docs->docs[a], *db = &docs->docs[b];
    switch ((enum pda_docs_sort)docs->sort) {
    case PDA_DOCS_BY_MODIFIED:
        return da->modified > db->modified ? -1 : da->modified < db->modified;
    case PDA_DOCS_BY_TITLE: {
        size_t la, lb;
        const char *ta = title_span(docs, a, &la), *tb = title_span(docs, b, &lb);
        if ((la == 0) != (lb == 0)) {
            return la == 0 ? 1 : -1; // untitled documents go last
        }
        for (size_t i = 0; i < la && i < lb; i++) {
            const int d = fold((unsigned char)ta[i]) - fold((unsigned char)tb[i]);
            if (d != 0) {
                return d;
            }
        }
        if (la != lb) {
            return la < lb ? -1 : 1;
        }
        return da->created > db->created ? -1 : da->created < db->created;
    }
    case PDA_DOCS_BY_CREATED:
    default:
        return da->created > db->created ? -1 : da->created < db->created;
    }
}

int pda_docs_order(const struct pda_docs *docs, int8_t order[PDA_DOCS_MAX]) {
    int n = 0;
    for (int i = 0; i < PDA_DOCS_MAX; i++) {
        if (!docs->docs[i].used) {
            continue;
        }
        // Insertion sort: at most 16 documents.
        int j = n++;
        while (j > 0 && compare(docs, order[j - 1], i) > 0) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = (int8_t)i;
    }
    return n;
}

void pda_docs_reset_keys(struct pda_docs *docs) {
    for (int i = 0; i < PDA_DOCS_MAX; i++) {
        pda_note_reset_keys(&docs->docs[i].note);
    }
}
