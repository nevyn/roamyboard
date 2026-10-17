/*
 * The PDA's shell: the home screen that launches apps, the title bar, and routing of key,
 * position and tick events to the open app. The PDA is always on the display; PDA mode only
 * gives it the keys (focus). Pure C, so that zmk/tests can run it on the host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <roamyboard/pda.h>

#include "apps.h"
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

/** Best results, kept in settings. */
struct pda_scores {
    uint32_t tetris;
    uint32_t snake;
    uint32_t rain;
    uint32_t breakout;
    uint16_t words_won;
    uint16_t words_played;
};

/** What the MIDI keys play, as the central sends it while a MIDI layer is on. */
struct pda_midi_view {
    /** The highest active layer has MIDI keys; the shell then shows this instead of the app. */
    bool shown;
    int8_t octave;
    int8_t transpose;
    /** 0 (pp) to 4 (ff). */
    uint8_t velocity_step;
    /** 0 is channel 1. */
    uint8_t channel;
    /** The latest note played, or -1. */
    int16_t last_note;
    /** Bit n % 8 of byte n / 8: note n is held. */
    uint8_t held[16];
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
    /**
     * The app last opened from the home screen, which the PDA opens again at startup, or
     * PDA_SHELL_HOME before any; going home and quick notes leave it.
     */
    int last_app;
    /** PDA_MOD_* bits of the modifier keys held now. */
    uint8_t held_mods;
    /** Uptime in milliseconds; the platform sets it before every call. */
    uint32_t now_ms;
    /**
     * How often the open app wants pda_shell_tick(), in milliseconds, or 0 for never. Apps set
     * it; opening another app or going home clears it.
     */
    uint32_t tick_ms;
    uint32_t random_state;

    struct pda_note note;
    struct pda_keys_state keys;
    struct pda_clock_app_state clock_app;
    struct pda_timer_state timer;
    struct pda_calc_state calc;
    struct pda_dice_state dice;
    struct pda_kana_state kana;
    struct pda_tetris_state tetris;
    struct pda_snake_state snake;
    struct pda_words_state words;
    struct pda_rain_state rain;
    struct pda_breakout_state breakout;

    struct pda_half_info left;
    struct pda_half_info right;
    struct pda_clock_state clock;
    struct pda_midi_view midi;
    struct pda_scores scores;
    /** The scores differ from settings. */
    bool scores_changed;
    /** Kana's weights and counts differ from settings. */
    bool kana_changed;

    /** The text of the latest PDA_SHELL_TYPE_OUT, which the app fills in. */
    char type_out[PDA_NOTE_MAX];
    size_t type_out_len;
};

/** What an app asks of the shell after an event. */
enum pda_app_result {
    PDA_APP_NONE,
    PDA_APP_REDRAW,
    /** Close the app: back home, or out of PDA mode if it was opened by quick note. */
    PDA_APP_CLOSE,
    /** Type type_out out to the host; PDA mode ends. */
    PDA_APP_TYPE_OUT,
};

struct pda_app {
    const char *name;
    /** Called when the app opens; may be NULL. */
    void (*open)(struct pda_shell *shell);
    /**
     * A key event of the HID keyboard page; usages 0xE0 to 0xE7 are the modifiers, which the
     * shell has already added to held_mods.
     */
    enum pda_app_result (*key)(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                               bool pressed);
    /** A key position event, or NULL if the app has no use for positions. */
    enum pda_app_result (*position)(struct pda_shell *shell, uint8_t position, bool pressed);
    /** Called every tick_ms while the app is open; may be NULL. */
    enum pda_app_result (*tick)(struct pda_shell *shell);
    /**
     * Ticks only while the PDA has focus: a game, which waits while the keys go to the host.
     * Without focus, pda_shell_tick_ms() is 0, so the platform does not wake for it.
     */
    bool focused_ticks;
    /** Draws the app into area, which lies below the title bar. */
    void (*draw)(const struct pda_shell *shell, struct pda_fb *fb, struct pda_rect area);
};

/** The apps, in home screen order; the digit keys open the first ten, 1 first and 0 last. */
extern const struct pda_app *const pda_apps[];
extern const int pda_app_count;
/** Index of the Note app, which quick note opens. */
extern const int pda_app_note;

/** Resets the shell; seed starts the random numbers that games use. */
void pda_shell_init(struct pda_shell *shell, uint32_t seed);

/** Columns of app icons on the home screen. */
#define PDA_HOME_COLUMNS 3

/**
 * Opens the app named name (as in pda_apps) without focusing the PDA, and makes it the last
 * app; for restoring last_app at startup. "" shows the home screen.
 * @return False, and nothing changed, if no app has that name.
 */
bool pda_shell_show(struct pda_shell *shell, const char *name);

/** last_app's name, or "" before any; pda_shell_show() takes it back. */
const char *pda_shell_last_app_name(const struct pda_shell *shell);

/**
 * Follows the central's mode. Slot 6 focuses the view as it is. Quick note opens the Note
 * app; when quick note ends, the view returns to the app that it replaced. Leaving PDA mode
 * keeps the view.
 */
enum pda_shell_action pda_shell_set_mode(struct pda_shell *shell, enum roamyboard_pda_mode mode);

/**
 * Takes the MIDI view from the central. While it is shown and the PDA has no focus, the shell
 * draws it in place of the app, which keeps running; focus shows the app again.
 */
enum pda_shell_action pda_shell_midi(struct pda_shell *shell, const struct pda_midi_view *view);

/** A key event from the central; see pda_note_key for the parameters. */
enum pda_shell_action pda_shell_key(struct pda_shell *shell, uint8_t usage, uint8_t implicit_mods,
                                    bool pressed);

/** A key position event from the central: row * 24 + keymap column. */
enum pda_shell_action pda_shell_position(struct pda_shell *shell, uint8_t position, bool pressed);

/** How often to call pda_shell_tick() now, in milliseconds, or 0 for not at all. */
uint32_t pda_shell_tick_ms(const struct pda_shell *shell);

/** The open app's tick; call it every pda_shell_tick_ms() milliseconds while that is not 0. */
enum pda_shell_action pda_shell_tick(struct pda_shell *shell);

/** A uniformly distributed number below n, which must be at least 1. */
uint32_t pda_shell_random(struct pda_shell *shell, uint32_t n);

/** The local time now, if the clock is set. */
bool pda_shell_time(const struct pda_shell *shell, struct pda_date_time *out);

/** Draws the whole view: the title bar, then home or the open app. */
void pda_shell_draw(const struct pda_shell *shell, struct pda_fb *fb);

/** Height of the title bar, including the rule under it. */
#define PDA_TITLE_H 22

/** A direction key: the arrows, W A S D, or H J K L. */
enum pda_dir {
    PDA_DIR_NONE,
    PDA_DIR_UP,
    PDA_DIR_DOWN,
    PDA_DIR_LEFT,
    PDA_DIR_RIGHT,
};

enum pda_dir pda_key_dir(uint8_t usage);

/** Draws text centred in area's width at y. */
void pda_text_center(struct pda_fb *fb, struct pda_rect area, int y, const char *text,
                     enum pda_font font, bool lit);

/**
 * Draws a framed box across area at y, cleared inside, with up to three centred lines of
 * body text; a NULL line ends the list early. Games use it for start, pause and game over.
 */
void pda_game_banner(struct pda_fb *fb, struct pda_rect area, int y, const char *const lines[3]);
