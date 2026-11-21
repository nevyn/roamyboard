/*
 * Pure logic of the roamyboard chain scan: sentinel search, key module count
 * stabilizing, physical-to-keymap column mapping and key event bookkeeping.
 * No Zephyr dependencies, so the host tests in zmk/tests compile it with cc.
 * docs/firmware.md describes the protocol.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Bit 7 of a chain byte: input H, grounded on every key module and high in the sentinel. */
#define CHAIN_SENTINEL_BIT 0x80

/** Bit 6 of a chain byte: input G, tied high on a pointing module and unused on key modules. */
#define CHAIN_MARKER_BIT 0x40

/** Most keys a key module can carry: inputs A to F of its 74HC165. */
#define CHAIN_MAX_ROWS 6

/** Longest chain that one scan can describe: one bit of chain.marked per module. */
#define CHAIN_MAX_MODULES 32

/** Pointing module buttons, as bits of the buttons output of chain_scan(). */
#define CHAIN_BUTTON_LEFT 0x01
#define CHAIN_BUTTON_RIGHT 0x02

/** Observed key module count of a scan that found no sentinel. */
#define CHAIN_COUNT_FAULT (-1)

/** Accepted key module count before any count has been stable. */
#define CHAIN_COUNT_UNKNOWN (-2)

/** Which end of the half the keymap columns are anchored to. */
enum chain_anchor {
    /**
     * Keymap column columns - 1 is the key module nearest the MCU module; further key
     * modules fill leftward. The mapping does not depend on the key module count.
     */
    CHAIN_ANCHOR_MCU,
    /**
     * Keymap column 0 is the key module nearest the terminator module; further key
     * modules fill rightward. The mapping depends on the key module count.
     */
    CHAIN_ANCHOR_TERMINATOR,
};

struct chain_config {
    enum chain_anchor anchor;
    /** Keymap columns that this half owns, >= 1. */
    int columns;
    /** Keys per key module, 1 to CHAIN_MAX_ROWS. Row 0 is the top key (bit rows - 1). */
    int rows;
    /** Consecutive identical observations needed before a new count is accepted, >= 1. */
    int stable_scans;
    /**
     * Consecutive observations of one count needed to leave an accepted fault. An unterminated
     * chain floats and can read like a terminated one for a few scans. Values below
     * stable_scans count as stable_scans.
     */
    int recovery_scans;
    /**
     * The half is mounted turned by 180 degrees in the plane of its keys, so the MCU module
     * sits at the other end of the half and SW1 is the top key. Mirrors both the column and
     * the row order, so that keymap columns and rows stay where the user sees them.
     */
    bool rotated;
};

/** Called once per key event, with keymap coordinates within this half. */
typedef void (*chain_event_fn)(void *ctx, int row, int column, bool pressed);

struct chain {
    struct chain_config config;
    /**
     * Modules in front of the sentinel (key modules and pointing modules) accepted by the
     * stabilizer, or CHAIN_COUNT_FAULT or CHAIN_COUNT_UNKNOWN.
     */
    int accepted;
    /** Pointing modules among the accepted modules: bit i is physical position i. */
    uint32_t marked;
    /** Module count seen by the latest scan, or CHAIN_COUNT_FAULT, and its pointing modules. */
    int observed;
    uint32_t observed_marked;
    /** Value that the stabilizer is counting towards, and for how many scans in a row. */
    int candidate;
    uint32_t candidate_marked;
    int streak;
    /** Keys reported pressed to the caller; rows * columns entries, row-major. */
    bool *reported;
};

enum chain_scan_result {
    /** The chain is stable: the active array holds this scan's key states. */
    CHAIN_SCAN_KEYS,
    /** The observed count differs from the accepted one: emit no key events. */
    CHAIN_SCAN_SETTLING,
    /** The accepted state is a fault (no sentinel): emit no key events. */
    CHAIN_SCAN_FAULT,
    /**
     * This scan made a new count (or a fault) the accepted one. The caller must call
     * chain_release_all() and reset its debouncers before handling further scans.
     */
    CHAIN_SCAN_REMAP,
};

/**
 * Prepares a chain for scanning. Nothing is accepted until config->stable_scans
 * identical observations have been made.
 *
 * @param reported Storage for config->rows * config->columns flags, owned by the caller.
 */
void chain_init(struct chain *chain, const struct chain_config *config, bool *reported);

/**
 * Counts the modules in one raw read of the chain.
 *
 * @param buf Bytes as read MSB-first from the chain, nearest module first.
 * @param len Number of bytes in buf; read one more than the longest chain expected.
 * @return The index of the first byte with CHAIN_SENTINEL_BIT set, which is the number
 *         of modules (key modules and pointing modules) in front of the terminator module,
 *         or CHAIN_COUNT_FAULT if no byte in buf has it set.
 */
int chain_count_key_modules(const uint8_t *buf, size_t len);

/**
 * Finds the pointing modules among the first count bytes of a read.
 *
 * @return A mask with bit i set when byte i has CHAIN_MARKER_BIT set. Bytes beyond
 *         CHAIN_MAX_MODULES are not examined.
 */
uint32_t chain_marked_modules(const uint8_t *buf, int count);

/**
 * Number of key modules that the chain has accepted, pointing modules left out; this is
 * the count that the status screen shows. Passes CHAIN_COUNT_FAULT and CHAIN_COUNT_UNKNOWN
 * through.
 */
int chain_key_module_count(const struct chain *chain);

/**
 * Maps a physical column to a keymap column.
 *
 * @param count Number of key modules on the chain; only the terminator anchor uses it.
 * @param physical Physical column, 0 being the key module nearest the MCU module. Pointing
 *                 modules take no physical column.
 * @return Keymap column within this half, or -1 if the key module falls outside
 *         config->columns and must be ignored.
 */
int chain_keymap_column(const struct chain_config *config, int count, int physical);

/**
 * Feeds one raw read of the chain through the sentinel search and the stabilizer and,
 * when the chain is stable, decodes the key states. The stabilizer counts a change in
 * which positions hold pointing modules as a new observation, like a change in count.
 *
 * @param buf Bytes as read from the chain, as for chain_count_key_modules().
 * @param active Output of rows * columns flags, row-major in keymap coordinates: true for
 *               each key whose switch is closed in this read. Written only when the
 *               result is CHAIN_SCAN_KEYS.
 * @param buttons Output of CHAIN_BUTTON_* bits for the closed switches of every pointing
 *                module, as the user sees them: SW5, on the MCU module's side, is
 *                CHAIN_BUTTON_LEFT on a rotated half and CHAIN_BUTTON_RIGHT on an
 *                unrotated one, and SW4 is the other button. Written only when the
 *                result is CHAIN_SCAN_KEYS.
 */
enum chain_scan_result chain_scan(struct chain *chain, const uint8_t *buf, size_t len,
                                  bool *active, uint8_t *buttons);

/**
 * Reports the keys whose state differs from what was last reported, and remembers the
 * new states.
 *
 * @param pressed rows * columns flags, row-major: the debounced state of each key.
 */
void chain_report(struct chain *chain, const bool *pressed, chain_event_fn emit, void *ctx);

/** Reports a release for every key that is currently reported pressed. */
void chain_release_all(struct chain *chain, chain_event_fn emit, void *ctx);
