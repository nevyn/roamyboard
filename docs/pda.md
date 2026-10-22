# Micro PDA

The micro PDA runs on the split peripheral (the right half) and is always on its nice!view while the central (the left half) is connected. While PDA mode is on, the PDA has focus: the central sends its keyboard keys to the PDA instead of the host. Leaving PDA mode keeps the PDA's view on the display, so an app stays visible while the keys go to the host. The PDA's home screen launches its apps: Note, Clock, Timer, Calc, Dice, Kana, Cat, the games Tetris, Snake, Bricks, Words, Rain and Anarch, Keys and Info.

## Keys

Both keys sit on the System layer of `zmk/config/roamyboard_split.keymap`:

- **PDA** (`&pda_sel`, the 6 key) selects the PDA as slot 6, a sixth device beside the five Bluetooth profiles. The PDA keeps the view that it shows.
- **Note** (`&pda_note`, the 7 key) opens a quick note: a new note in the Note app, which Escape or Cmd-Return closes, back to the host and to the view that the note replaced. A quick note left empty is not kept.

Pressing the key of the mode that is on returns the keys to the host. Choosing a Bluetooth profile or an output (`&bt BT_SEL`, `BT_NXT`, `BT_PRV`, `&out`) also returns them, as choosing another device would, even the profile or output that is already chosen. ZMK raises no event for that, so the central looks up the binding of every key pressed in PDA mode, once the keymap has handled it (after hold-tap has decided a layer key). A host that connects or disconnects meanwhile does not return the keys.

The home screen shows the apps as a grid of icons, three to a row, with the highlighted app's name under the grid. The arrow keys, W A S D or H J K L move the highlight: sideways by one, wrapping from row to row, and up or down by a row. Return or Space opens the highlighted app, and the digits (or the keypad's) open the first ten directly, 1 the first and 0 the tenth. A grid taller than the display scrolls by rows, with a mark at its right edge where it goes on. In an app, Escape returns to the home screen; on the home screen, it returns the keys to the host. An app keeps its state when it closes: a game or a running timer is where it was when it opens again.

The PDA remembers what it shows: as soon as the view changes, the peripheral writes the app's name to the settings key `roamyboard/pda/app` (empty for the home screen), and shows that view again when it starts. A quick note counts as the view that it replaced. The icons are 16 × 16 pixel art in `zmk/tools/pda_icons.py`, which generates `zmk/src/pda/icons.c`; the shell finds an app's icon by its name in lowercase.

? (Shift-/) shows help over the home screen or any app: its keys, each with what it does, as an inverted key label and an explanation under it. The apps show no key legends of their own; a screen that would need one says "? help". Up and Down (W S, K J) scroll the help, and Escape closes it. Games wait while it shows; other apps, such as the Cat and the Timer, go on. Leaving PDA mode or changing the view closes it. In a note, ? is a question mark: an app's `types_question_mark` keeps ? for itself. Each app's help is the `help` string of its `struct pda_app`; the home screen's is `pda_home_help` in `shell.c`, and `zmk/src/pda/help.c` draws it.

The games start, and start again after a game over, with Space or Return.

Consumer keys, such as volume and media keys, still reach the host in PDA mode.

## Apps

**Note.** Up to 16 notes of up to 512 bytes each, kept across restarts. The app opens on the note list: a row per note, titled by its first line ("Untitled" when that is empty), with the sort order and the count above it.

| Key, in the list | Does |
|---|---|
| Up, Down (W S, K J) | Move the highlight. |
| Return, Space, Right | Open the highlighted note. |
| Cmd-N | Makes a new note and opens it. With 16 notes, the list says to delete one first. |
| Cmd-Backspace | Deletes the highlighted note, after Return confirms; any other key keeps it. |
| Cmd-L | Sorts by creation (newest first), by last edit (latest first), or by title (A to Z, ignoring case, untitled last), in turn. |
| Escape | Closes the app. |

| Key, in a note | Does |
|---|---|
| Letters, digits, punctuation, Space, Tab | Type themselves, with Shift. |
| Return, keypad Enter | Start a new line. |
| Backspace | Deletes the last character. |
| Cmd-Backspace | Deletes the note, after Return confirms, and goes back to the list. |
| Cmd-N | Leaves the note for a new one. |
| Cmd-Return | Returns the keys to the host and types the note out to it; the note stays. |
| Escape | Back to the list; in a quick note, back to the host. A note left empty is deleted. |
| å, ä, ö (Keypad layer) | Type themselves: the PDA reads the keymap's Option dead keys back into the letters. |

The keyboard may have no clock, so a note carries stamps from a counter that only grows, instead of dates: one when it is made, and one at every edit. Sorting by creation or by edit compares the stamps. `zmk/src/pda/docs.c` holds the notes and their order; the app in `apps.c` draws the list and the editor.

**Clock.** The time in seven-segment digits, the seconds, the weekday, the date and the ISO week, once a host has given the time (Clock, below).

**Timer.** A stopwatch, or a countdown: the digits type its minutes, P sets 25 minutes (a pomodoro), B 5 minutes (a break), S the stopwatch. Space or Return starts and pauses it, Backspace sets it back to zero, or deletes the last digit typed. It keeps counting while the keys go to the host, and a finished countdown inverts its digits.

**Calc.** An RPN calculator with a four-level stack, as on HP's: Return or Space pushes X; + - * / combine Y and X (Shift-= and Shift-8 on the main keys, or the keypad's); N negates, S swaps X and Y, R takes the square root, C clears; Backspace deletes a digit, or clears X. Cmd-Return types X out to the host.

**Dice.** Type a roll such as `2d6+3`, `d20` or `4d6-1`, then Return; the total shows in large digits, the single rolls under it, and the three rolls before it at the bottom. Space rolls a d20, and Return on an empty line rolls the latest roll again. Up to 20 dice of 2 to 1000 sides.

**Kana.** A drill of the 46 basic hiragana and katakana: type a kana's romaji (Hepburn; si, ti, tu, hu and o for を also count). The kana fills its box when right; when wrong, its box is framed and the answer shows. Any key goes on to the next kana, and a letter starts its answer. A kana answered wrong comes back more often, one answered right less; the weights, the counts and the longest run are kept in the settings key `roamyboard/pda/kana`, written when PDA mode ends. Tab switches between hiragana, katakana and both.

The kana come from `zmk/src/display/font_kana_40.c`: Noto Sans JP at 40 px, 1 bit per pixel, only the 92 kana, made by `zmk/tools/pda_kana_font.sh`. Noto Sans JP is under the SIL Open Font License 1.1; `font_kana_40.LICENSE` holds its copyright notice and the license.

**Cat.** The status screen's cat comes over to play. Opening the app sends the left half's cat dashing off its right edge, and half a second later she runs in from the left edge of a playground on the right half: a cat tree with a bed on its top shelf and a lower shelf by the window, a sisal post, and a bowl on the floor. She lives there while the app is open, whether the PDA has focus or not, and the left half's box stays empty. Closing the app sends her back.

Three meters along the top show her needs: food, fun and energy, each 0 to 100. Food drops by one every 90 s, fun every 60 s, and energy every 20 s while she is awake; sleep restores 2 energy every 10 s, a loaf 1. Below 25 energy she goes to bed and sleeps until she is full of energy; below 35 food she goes to the bowl, and if it is empty she sits by it asking for food. Otherwise she picks something now and then: roams to a spot on any level, jumping from shelf to shelf, sits and blinks, loafs, watches the window, stretches, runs zoomies along the floor, naps, or plays with a ball that lies around. The needs never kill her. They are kept in the settings key `roamyboard/pda/cat`, written when the tens of a need or the bowl change, and when PDA mode ends; the time she spends on the left half does not count.

| Key | Does |
|---|---|
| F | Fills the bowl with three portions; she comes to eat if she is hungry. Each portion gives 35 food. |
| Space | Throws a ball in from the top right, or, while the laser dot is on, drops it at the dot, so the dot aims it. She chases and bats it along the floor a few times, then loses interest; it stays for later. |
| Arrows, W A S D, H J K L | Move a laser dot; the first press puts it on the floor. A held key starts it at 20 px/s and speeds it up by 260 px/s² to 130 px/s, two keys move it diagonally, and on release it glides to a stop over about a second, losing speed by a factor e every 250 ms, so a tap nudges it a few pixels. She stalks a dot on a surface and pounces on it, sits under a dot on the wall and leaps at it if it is within reach, and follows it from level to level. The dot goes out 6 s after it stops. |
| Return | Pets her: a heart rises, fun goes up by 5, and she purrs with her eyes closed. |
| Tab | Toy mode on or off (below). |
| Escape | Sends her home: she runs off the left edge, then the app closes. A second Escape closes it at once. |

In toy mode the meters are hidden and nothing is at stake, like a screensaver: her kept needs wait unchanged, and a separate set leads her, in which food and fun never fall below 60, so she never begs, while energy runs as usual, so she still naps and goes to bed. Toy mode lasts until Tab again, and is kept in `roamyboard/pda/cat` with the needs; the toy needs start afresh each time.

A sleeping cat ignores the ball below 40 energy and the laser below 30. A quick note does not move her: she is where she was when the note closes. The playground redraws only when something on it changes; while she sits, sleeps or loafs, the app ticks four times a second, and 25 times while she, the ball or the laser dot moves. Her frames come from `zmk/tools/cat_frames.py`, which writes them upright for the PDA's framebuffer to `zmk/src/pda/cat_sprites.c`, with a mask of each frame's outline so that she hides what lies behind her.

**Tetris.** A 10 × 20 well. Left and Right (or A D, H L) move the piece, Up (W, K) or X turns it clockwise, Z anticlockwise, Down (S, J) drops it one row, Space drops it to the bottom, P pauses. The pieces come from shuffled bags of all seven; cleared lines score 100, 300, 500 or 800 times the level plus one; the level starts at 0, rises every 10 lines and speeds the fall.

**Snake.** A 17 × 32 field walled by the display. The direction keys steer; each piece of food grows the snake by two cells and speeds it up.

**Bricks.** Breakout: Left and Right move the paddle while they are held, Space serves the ball. The paddle's edges send the ball off steeper. A brick scores more the higher its row; clearing the wall builds a new one and speeds the ball up. Three balls.

**Words.** Guess the five-letter word in six tries: a filled cell is the right letter in the right place, a framed cell a letter that the word has elsewhere. Any five letters count as a guess, since the PDA has no dictionary. Under the grid, the letters not yet ruled out. The answers come from `zmk/src/pda/words.c`, which `zmk/tools/pda_words.py` generates.

**Rain.** Words fall; typing one clears it. The first letter picks the lowest word that starts with it, and a wrong letter lets the word go. A word that reaches the bottom costs one of three lives. The words fall faster and come more often as the score grows.

**Anarch.** A Doom-like shooter: drummyfish's [Anarch](https://gitlab.com/drummyfish/anarch), with its ten levels. It fills the whole display without the title bar, sideways: the game's top is the display's right edge, so it reads upright with the head tilted to the right. The game draws 160 × 68 pixels in its 256 colours, and `zmk/src/pda/anarch.c` dithers each pixel to 1 bit by its colour's brightness, with a 4 × 4 Bayer matrix. It has no sound, since the keyboard has no speaker.

| Key | Does |
|---|---|
| Up, Down, W S | Walk. |
| Left, Right | Turn. |
| A D | Strafe. |
| Space, Return | Fire; in the menu, choose. |
| Shift, Backspace | In the menu, back. Held, Left and Right strafe, and Up and Down look if free look is on in the game's settings. |
| Q E | Previous and next weapon. |
| Tab | The map, while held. |
| J | Jump. |
| M | The game's menu. |
| Escape | Closes the app; the game waits where it is. |

The game runs on a thread of its own at the lowest priority (`zmk/src/pda/anarch_runner.c`), 25 frames a second, so that a frame never holds up ZMK's work queues; the app feeds it the held keys every 20 ms and takes its newest frame. The game runs only while the feeds come, and pauses 100 ms after the last one: that is how it waits while the keys go to the host, the help shows or another app opens, and it resumes without catching up on the pause. A key released while the PDA had no focus never reaches the app, so after a pause the app forgets every held key. On the host, `zmk/src/pda/anarch_inline.c` runs the game inside the app's tick instead, so that the tests and the screen mock drive it by key and tick, as they do the other games.

The game keeps the levels reached and its settings in 12 bytes, which the peripheral writes to `roamyboard/pda/anarch` when PDA mode ends. `zmk/src/pda/anarch/` holds Anarch's game headers unchanged, from commit 6f905621 (2023-09-08) of its repository; its code and art are under CC0 1.0 (`anarch/LICENSE`). They take about 100 KB of flash, 20 KB of code and 80 KB of levels and art, and the app with its thread about 11 KB of RAM.

**Keys.** The Keys app shows a cell per key position, with keymap columns from top to bottom and rows from left to right, and a gap between the halves: filled while held, outlined once pressed, a dot before that. The line at the bottom names the latest position (`r2c13`: row 2, keymap column 13) and its HID usage in hex. It helps to check a new build's key modules. Cmd-Backspace starts over.

**Info.** The Info app shows each half's key module count, battery and build ID, and where the clock comes from: `time BT2` for the host on Bluetooth profile 2, `no CTS` when no connected host offers the time, `time err` when a read failed, `time ...` before any host was asked.

The games wait while the keys go to the host, under a "paused" banner. Their best scores (and Words' won and played counts) are kept in the settings key `roamyboard/pda/scores`, written when PDA mode ends.

## Displays

The left half shows "PDA" or "note" in place of the host name while the PDA has focus. The right half shows the PDA instead of its status screen while the left half is connected; without it, the status screen returns, with the local OTA key's hint. The PDA draws dark on light like the status screen, and inverts while it has focus, so that PDA mode cannot be mistaken for typing to the host. Its title bar shows the app and the right half's battery; an app with `fullscreen` set, such as Anarch, draws on the whole display instead. While a MIDI layer is on and the PDA has no focus, the right half shows the MIDI view in place of the app, which keeps running ([MIDI](midi.md)). `zmk/tools/screen_mock/run.sh` renders the PDA as `peripheral-pda-*`, from the shell's own drawing code.

The PDA draws into a 1-bit framebuffer of the whole display, 68 × 160 pixels (`zmk/src/pda/gfx.h`): apps draw shapes in pure C and text through `pda_text_draw()`, one line cut at its width, which `zmk/src/display/pda_text.c` renders with LVGL's fonts on a hidden scratch canvas. `roamyboard_pda_render()` runs the drawing on the display work queue, and `screen_draw_pda()` copies the framebuffer onto the three canvases. The shell and the apps (`zmk/src/pda/shell.c`, `apps.c`, `app_*.c`, `game_*.c`) are pure C; `zmk/tests/pda_shell_test.c` and `pda_apps_test.c` drive them by key and tick, with stubbed text.

An app that runs on its own, such as Timer or a game, sets the shell's `tick_ms`; the peripheral then calls `pda_shell_tick()` that often while the app is open, focused or not, and redraws only when the app asks. A redraw writes only the canvases whose pixels changed.

LVGL's built-in Montserrat has ASCII only, so the note uses `zmk/src/display/font_pda_12.c`: Montserrat 12 with Latin-1 (U+0020 to U+007E and U+00A0 to U+00FF), made by LVGL's `lv_font_conv` with the options in the file's header, from `scripts/built_in_font/Montserrat-Medium.ttf` in LVGL's repository. To add characters, run it again with a wider `-r`.

## Key capture

`zmk/src/pda/central.c` subscribes to `zmk_keycode_state_changed`. In PDA mode, it sends every event of the HID keyboard page to the peripheral and returns `ZMK_EV_EVENT_HANDLED`, so ZMK's `hid_listener` never sees it. Entering PDA mode first clears the HID reports, so no key stays held on the host. The PDA receives resolved keycodes, after layers and macros, so it sees the keymap as typed, including the Keypad layer and Shift.

ZMK calls listeners in the order of the `.event_subscription` section, which is link order (`app/include/linker/zmk-events.ld` keeps the section unsorted). The module's sources join ZMK's `app` library before ZMK's own, so the PDA's subscription lies before `hid_listener`'s. `roamyboard_pda_select()` compares the two subscriptions' addresses and refuses PDA mode, with an error in the log, if that ever changes; then keys would reach the host as well as the PDA.

## Split data channel

ZMK's split link carries a fixed set of messages, so the PDA brings its own GATT service on the peripheral (`zmk/src/pda/peripheral.c`, UUIDs in `zmk/src/pda/link.h`). The central, which is already the GATT client on the split connection, discovers it once the link is encrypted, and subscribes. Both characteristics require encryption; the split link is bonded.

| Direction | Characteristic | Message | Bytes |
|---|---|---|---|
| Central to peripheral | write without response | Mode | `01`, mode (0 off, 1 slot 6, 2 note) |
| | | Key | `02`, pressed, implicit modifier bits, HID usage ID |
| | | Position | `03`, pressed, key position; in PDA mode only |
| | | Info | `04`, key module count (signed), battery percent, build ID; when the link comes up and when either changes |
| | | Time | `05`, clock status, profile, local seconds since 2000-01-01 (uint32 little-endian); sent with Info and when the clock changes |
| | | MIDI | `06`, shown, octave, transpose (both int8), velocity step, channel, latest note or `FF`, then 12 bytes of held notes from 24 (C1) to 119, a bit each; on every MIDI key while a MIDI layer is on ([MIDI](midi.md)) |
| Peripheral to central | notify | Text | `81`, UTF-8 bytes; appends to the text to type out |
| | | Type out | `82`; leave PDA mode, then type the text |
| | | Leave | `83`; leave PDA mode |
| | | Cat | `84`, here; whether the cat is on the peripheral (the Cat app is the view); when that changes and in answer to Info |

Writes and notifications carry at most 20 bytes, which fits the smallest ATT MTU. A disconnect of the split link leaves PDA mode on both halves, and brings the cat back to the central's status screen.

On the central, `roamyboard_pda_cat_away()` returns the latest Cat message, and `roamyboard_pda_changed` follows each change; the status screen then calls `cat_set_away()`, which sends its cat dashing off the right edge or back in from it. The halves' displays sit next to each other at the inner ends, so she seems to cross from one to the other.

## Clock

The home screen's title shows the time, HH:MM, once a host has given it. `zmk/src/pda/host_time.c` on the central reads the Current Time characteristic (0x2A2B) of the Current Time Service from the connected hosts, in profile order, until one answers: whenever a host connection is encrypted, and every hour. The characteristic holds the host's local time, so the clock needs no time zone. The central keeps the time running on its uptime and sends it to the peripheral, which runs it on its own uptime and redraws each minute. Between reads, the clock drifts with the nice!nanos' 32 kHz crystals.

An iPhone serves the time; macOS does not (tried 2026-10-12). Connecting the phone for a few seconds sets the clock, and the clock keeps running when the phone goes: a later read that finds no host with the time leaves it as it is. The Info app shows which profile gave the time.

## Typing out

The central types the text as `zmk_keycode_state_changed` events, one press or release every `CONFIG_ROAMYBOARD_PDA_TYPE_OUT_STEP_MS` (10 ms by default), so a 500-byte note takes about 10 s. It assumes the host's layout is ABC - Extended, as the keymaps do: å, ä and ö go out as Option-K or Option-U and the letter. Characters with no key on that layout are skipped with a warning in the log. Entering PDA mode stops a type-out in progress.

## Storage

The peripheral keeps each note in the settings key `roamyboard/pda/doc/<slot>`, slot 0 to 15: its two stamps as little-endian uint32s, then at most 512 bytes of UTF-8. The list's sort is in `roamyboard/pda/docsort`. A build before the note list kept one note in `roamyboard/pda/note`; at startup that note becomes the first note of an empty list, and the key goes at the next save. The games' best scores are in in `roamyboard/pda/scores`, the Kana drill's weights in `roamyboard/pda/kana`, the cat's needs in `roamyboard/pda/cat`, and Anarch's save in `roamyboard/pda/anarch`. It writes them when PDA mode ends, and only what changed; the cat's needs also while the app runs (Cat, above).

Settings live in a 32 KB flash partition, 8 sectors of 4 KB, of which Zephyr's NVS keeps one free for garbage collection. 16 full notes take about 8.5 KB of it, and the same in RAM, since the PDA holds every note in memory.

## Latency

ZMK lets the split peripheral skip up to `CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY` (30) connection events to save power, so a message from the central waits until the peripheral listens: up to about 225 ms, about 110 ms on average. While the PDA has focus, the central asks for peripheral latency 0, so that the right half listens at every 7.5 ms connection event; when focus ends, it asks for ZMK's latency again.

## Known gaps

- Only the split build has the PDA; the unibody has no peripheral.
- Only hosts with a Current Time Service set the clock; a companion app that sets it from any host is on the backlog.
