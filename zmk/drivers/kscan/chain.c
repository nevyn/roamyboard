/*
 * SPDX-License-Identifier: MIT
 */

#include "chain.h"

#include <string.h>

static size_t key_count(const struct chain_config *config) {
    return (size_t)config->rows * (size_t)config->columns;
}

void chain_init(struct chain *chain, const struct chain_config *config, bool *reported) {
    chain->config = *config;
    chain->accepted = CHAIN_COUNT_UNKNOWN;
    chain->observed = CHAIN_COUNT_UNKNOWN;
    chain->candidate = CHAIN_COUNT_UNKNOWN;
    chain->streak = 0;
    chain->reported = reported;
    memset(reported, 0, key_count(config) * sizeof(bool));
}

int chain_count_modules(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] & CHAIN_SENTINEL_BIT) {
            return (int)i;
        }
    }
    return CHAIN_COUNT_FAULT;
}

int chain_keymap_column(const struct chain_config *config, int count, int physical) {
    const int column = config->anchor == CHAIN_ANCHOR_MCU ? config->columns - 1 - physical
                                                          : count - 1 - physical;
    return (column >= 0 && column < config->columns) ? column : -1;
}

static enum chain_scan_result stabilize(struct chain *chain, int observed) {
    chain->observed = observed;

    if (observed == chain->accepted) {
        chain->candidate = observed;
        chain->streak = 0;
        return observed == CHAIN_COUNT_FAULT ? CHAIN_SCAN_FAULT : CHAIN_SCAN_KEYS;
    }

    if (observed == chain->candidate) {
        chain->streak++;
    } else {
        chain->candidate = observed;
        chain->streak = 1;
    }

    if (chain->streak < chain->config.stable_scans) {
        return CHAIN_SCAN_SETTLING;
    }

    chain->accepted = observed;
    chain->streak = 0;
    return CHAIN_SCAN_REMAP;
}

enum chain_scan_result chain_scan(struct chain *chain, const uint8_t *buf, size_t len,
                                  bool *active) {
    const struct chain_config *config = &chain->config;
    const enum chain_scan_result result = stabilize(chain, chain_count_modules(buf, len));
    if (result != CHAIN_SCAN_KEYS) {
        return result;
    }

    memset(active, 0, key_count(config) * sizeof(bool));
    for (int physical = 0; physical < chain->accepted; physical++) {
        const int column = chain_keymap_column(config, chain->accepted, physical);
        if (column < 0) {
            continue;
        }
        for (int row = 0; row < config->rows; row++) {
            const int bit = config->rows - 1 - row;
            active[row * config->columns + column] = (buf[physical] >> bit) & 1;
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
