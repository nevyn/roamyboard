/*
 * The PDA's shell: the home screen that launches apps, the title bar, and routing of key and
 * position events to the open app. The PDA is always on the display; PDA mode only gives it
 * the keys (focus). Pure C, so that zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <roamyboard/pda.h>

#include "clock.h"
#include "gfx.h"
#include "note.h"

/** One half's general state, for the Info app. */
struct pda_half_info {
    /** Whether the other fields have arrived; the right half always knows its own. */
    bool known;
    /** Key module count, or ROAMYBOARD_CHAIN_FAULT or ROAMYBOARD_CHAIN_UNKNOWN. */
    int key_module_count;
    /** State of charge in percent. */
    uint8_t battery;
    /** NUL-terminated build ID. */
    char build_id[16];
};

/** The Keys app: which key positions are held now and which have been pressed. */
struct pda_keys_state {
    uint8_t held[16];
    uint8_t seen[16];
    /** The latest pressed position and HID usage, or -1. */
    int last_position;
    int last_usage;
};

/** The clock as the central last reported it. */
struct pda_clock_state {
    enum pda_clock_status status;
    /** The profile whose host gave the time. */
    uint8_t profile;
    /** Local seconds since 2000-01-01 at at_ms. */
    uint32_t seconds;
    /** Uptime in milliseconds when the report arrived. */
    uint32_t at_ms;
};

/** What the platform must do after an event. */
enum pda_shell_action {
    PDA_SHELL_NONE,
    /** Redraw the view. */
    PDA_SHELL_REDRAW,
    /** Type pda_shell.type_out out to the host; PDA mode ends. */
    PDA_SHELL_TYPE_OUT,
    /** Return the keys to the host. */
    PDA_SHELL_LEAVE,
};

/** Index of no app: the home screen. */
#define PDA_SHELL_HOME (-1)

struct pda_shell {
    /** ROAMYBOARD_PDA_OFF while the keys go to the host; the view stays. */
    enum roamyboard_pda_mode mode;
    /** The open app's index in pda_apps, or PDA_SHELL_HOME. */
    int app;
    /** The highlighted app on the home screen. */
    int selected;
    /** The app was opened by quick note, so Escape leaves PDA mode instead of going home. */
    bool quick;
    /** The app that quick note replaced, PDA_SHELL_HOME included; it returns afterwards. */
    int before_quick;
    struct pda_note note;
    struct pda_keys_state keys;
    struct pda_half_info left;
    struct pda_half_info right;
    struct pda_clock_state clock;
    /** Uptime in milliseconds; the platform sets it before drawing, for the clock. */
    uint32_t now_ms;
    /** The text of the latest PDA_SHELL_TYPE_OUT. */
    char type_out[PDA_NOTE_MAX];
    size_t type_out_len;
};

/** What an app asks of the shell after a key event. */
enum pda_app_result {
    PDA_APP_NONE,
    PDA_APP_REDRAW,
    /** Close the app: back home, or out of PDA mode if it was opened by quick note. */
    PDA_APP_CLOSE,
    /** Copy the note to type_out and leave PDA mode. */
    PDA_APP_TYPE_OUT,
};

struct pda_app {
    const char *name;
    /** Called when the app opens. */
    void (*open)(struct pda_shell *shell);
    /** A key event of the HID keyboard page; usage 0xE0 to 0xE7 are the modifiers. */
    enum pda_app_result (*key)(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                               bool pressed);
    /** A key position event, or NULL if the app has no use for positions. */
    enum pda_app_result (*position)(struct pda_shell *shell, uint8_t position, bool pressed);
    /** Draws the app into area, which lies below the title bar. */
    void (*draw)(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area);
};

/** The apps, in home screen order; the digit keys open them by position, 1 first. */
extern const struct pda_app *const pda_apps[];
extern const int pda_app_count;
/** Index of the Note app, which quick note opens. */
extern const int pda_app_note;

void pda_shell_init(struct pda_shell *shell);

/**
 * Follows the central's mode. Slot 6 focuses the view as it is. Quick note opens the Note
 * app; when quick note ends, the view returns to the app that it replaced. Leaving PDA mode
 * keeps the view.
 */
enum pda_shell_action pda_shell_set_mode(struct pda_shell *shell, enum roamyboard_pda_mode mode);

/** A key event from the central; see pda_note_key for the parameters. */
enum pda_shell_action pda_shell_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed);

/** A key position event from the central: row * 24 + keymap column. */
enum pda_shell_action pda_shell_position(struct pda_shell *shell, uint8_t position, bool pressed);

/** The local time now, if the clock is set. */
bool pda_shell_time(const struct pda_shell *shell, struct pda_date_time *out);

/** Draws the whole view: the title bar, then home or the open app. */
void pda_shell_draw(const struct pda_shell *shell, struct pda_fb *fb);

/** Height of the title bar, including the rule under it. */
#define PDA_TITLE_H 22
