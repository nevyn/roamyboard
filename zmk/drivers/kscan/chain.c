/*
 * SPDX-License-Identifier: MIT
 */

#include "chain.h"

#include <string.h>

/** Inputs E and D of a module's 74HC165. */
#define SW5_BIT 0x10
#define SW4_BIT 0x08

static size_t key_count(const struct chain_config *config) {
    return (size_t)config->rows * (size_t)config->columns;
}

void chain_init(struct chain *chain, const struct chain_config *config, bool *reported) {
    chain->config = *config;
    chain->accepted = CHAIN_COUNT_UNKNOWN;
    chain->marked = 0;
    chain->observed = CHAIN_COUNT_UNKNOWN;
    chain->observed_marked = 0;
    chain->candidate = CHAIN_COUNT_UNKNOWN;
    chain->candidate_marked = 0;
    chain->streak = 0;
    chain->reported = reported;
    memset(reported, 0, key_count(config) * sizeof(bool));
}

int chain_count_key_modules(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] & CHAIN_SENTINEL_BIT) {
            return (int)i;
        }
    }
    return CHAIN_COUNT_FAULT;
}

uint32_t chain_marked_modules(const uint8_t *buf, int count) {
    uint32_t marked = 0;
    for (int i = 0; i < count && i < CHAIN_MAX_MODULES; i++) {
        if (buf[i] & CHAIN_MARKER_BIT) {
            marked |= 1u << i;
        }
    }
    return marked;
}

int chain_key_module_count(const struct chain *chain) {
    if (chain->accepted < 0) {
        return chain->accepted;
    }
    return chain->accepted - __builtin_popcount(chain->marked);
}

int chain_keymap_column(const struct chain_config *config, int count, int physical) {
    const int unrotated = config->anchor == CHAIN_ANCHOR_MCU ? config->columns - 1 - physical
                                                             : count - 1 - physical;
    const int column = config->rotated ? config->columns - 1 - unrotated : unrotated;
    return (column >= 0 && column < config->columns) ? column : -1;
}

static enum chain_scan_result stabilize(struct chain *chain, int observed, uint32_t marked) {
    chain->observed = observed;
    chain->observed_marked = marked;

    if (observed == chain->accepted && marked == chain->marked) {
        chain->candidate = observed;
        chain->candidate_marked = marked;
        chain->streak = 0;
        return observed == CHAIN_COUNT_FAULT ? CHAIN_SCAN_FAULT : CHAIN_SCAN_KEYS;
    }

    if (observed == chain->candidate && marked == chain->candidate_marked) {
        chain->streak++;
    } else {
        chain->candidate = observed;
        chain->candidate_marked = marked;
        chain->streak = 1;
    }

    const int needed = chain->accepted == CHAIN_COUNT_FAULT && chain->config.recovery_scans > chain->config.stable_scans
                           ? chain->config.recovery_scans
                           : chain->config.stable_scans;
    if (chain->streak < needed) {
        return CHAIN_SCAN_SETTLING;
    }

    chain->accepted = observed;
    chain->marked = marked;
    chain->streak = 0;
    return CHAIN_SCAN_REMAP;
}

enum chain_scan_result chain_scan(struct chain *chain, const uint8_t *buf, size_t len,
                                  bool *active, uint8_t *buttons) {
    const struct chain_config *config = &chain->config;
    const int observed = chain_count_key_modules(buf, len);
    const enum chain_scan_result result =
        stabilize(chain, observed, chain_marked_modules(buf, observed));
    if (result != CHAIN_SCAN_KEYS) {
        return result;
    }

    // SW5 sits on the MCU module's side of the pointing module.
    const uint8_t sw5 = config->rotated ? CHAIN_BUTTON_LEFT : CHAIN_BUTTON_RIGHT;
    const uint8_t sw4 = config->rotated ? CHAIN_BUTTON_RIGHT : CHAIN_BUTTON_LEFT;
    const int key_modules = chain_key_module_count(chain);
    memset(active, 0, key_count(config) * sizeof(bool));
    *buttons = 0;
    int physical = 0;
    for (int position = 0; position < chain->accepted; position++) {
        const uint8_t byte = buf[position];
        if (position < CHAIN_MAX_MODULES && (chain->marked & (1u << position))) {
            *buttons |= (byte & SW5_BIT ? sw5 : 0) | (byte & SW4_BIT ? sw4 : 0);
            continue;
        }
        const int column = chain_keymap_column(config, key_modules, physical++);
        if (column < 0) {
            continue;
        }
        for (int row = 0; row < config->rows; row++) {
            const int bit = config->rotated ? row : config->rows - 1 - row;
            active[row * config->columns + column] = (byte >> bit) & 1;
        }
    }
    return CHAIN_SCAN_KEYS;
}

void chain_report(struct chain *chain, const bool *pressed, chain_event_fn emit, void *ctx) {
    const int columns = chain->config.columns;
    for (size_t i = 0; i < key_count(&chain->config); i++) {
        if (pressed[i] != chain->reported[i]) {
            chain->reported[i] = pressed[i];
            emit(ctx, (int)i / columns, (int)i % columns, pressed[i]);
        }
    }
}

void chain_release_all(struct chain *chain, chain_event_fn emit, void *ctx) {
    const int columns = chain->config.columns;
    for (size_t i = 0; i < key_count(&chain->config); i++) {
        if (chain->reported[i]) {
            chain->reported[i] = false;
            emit(ctx, (int)i / columns, (int)i % columns, false);
        }
    }
}
