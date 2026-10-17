# Micro PDA

The micro PDA runs on the split peripheral (the right half) and is always on its nice!view while the central (the left half) is connected. While PDA mode is on, the PDA has focus: the central sends its keyboard keys to the PDA instead of the host. Leaving PDA mode keeps the PDA's view on the display, so an app stays visible while the keys go to the host. The PDA's home screen launches its apps: Note, Clock, Timer, Calc, Dice, Kana, the games Tetris, Snake, Bricks, Words and Rain, Keys and Info.

## Keys

Both keys sit on the System layer of `zmk/config/roamyboard_split.keymap`:

- **PDA** (`&pda_sel`, the 6 key) selects the PDA as slot 6, a sixth device beside the five Bluetooth profiles. The PDA keeps the view that it shows.
- **Note** (`&pda_note`, the 7 key) opens a quick note: the Note app, which Escape or Cmd-Return closes, back to the host and to the view that the note replaced.

Pressing the key of the mode that is on returns the keys to the host. Choosing another Bluetooth profile or output (`&bt BT_SEL`, `&out`) also returns them, as choosing another device would. A host that connects or disconnects meanwhile does not, and neither does selecting the profile that is already chosen.

The home screen shows the apps as a grid of icons, three to a row, with the highlighted app's name under the grid. The arrow keys, W A S D or H J K L move the highlight: sideways by one, wrapping from row to row, and up or down by a row. Return or Space opens the highlighted app, and the digits (or the keypad's) open the first ten directly, 1 the first and 0 the tenth. A grid taller than the display scrolls by rows, with a mark at its right edge where it goes on. In an app, Escape returns to the home screen; on the home screen, it returns the keys to the host. An app keeps its state when it closes: a game or a running timer is where it was when it opens again.

The PDA remembers the app that it shows: when PDA mode ends, the peripheral writes the app's name to the settings key `roamyboard/pda/app` (empty for the home screen), and opens that app again when it starts. The icons are 16 × 16 pixel art in `zmk/tools/pda_icons.py`, which generates `zmk/src/pda/icons.c`; the shell finds an app's icon by its name in lowercase.

The games start, and start again after a game over, with Space or Return.

Consumer keys, such as volume and media keys, still reach the host in PDA mode.

## Apps

**Note.** A note, kept across restarts, that can be typed out to the host:

| Key | Does |
|---|---|
| Letters, digits, punctuation, Space, Tab | Type themselves, with Shift. |
| Return, keypad Enter | Start a new line. |
| Backspace | Deletes the last character. |
| Cmd-Backspace | Deletes the whole note. |
| Cmd-Return | Returns the keys to the host and types the note out to it, then empties the note. |
| Escape | Closes the note; it stays for next time. |
| å, ä, ö (Keypad layer) | Type themselves: the PDA reads the keymap's Option dead keys back into the letters. |

**Clock.** The time in seven-segment digits, the seconds, the weekday, the date and the ISO week, once a host has given the time (Clock, below).

**Timer.** A stopwatch, or a countdown: the digits type its minutes, P sets 25 minutes (a pomodoro), B 5 minutes (a break), S the stopwatch. Space or Return starts and pauses it, Backspace sets it back to zero, or deletes the last digit typed. It keeps counting while the keys go to the host, and a finished countdown inverts its digits.

**Calc.** An RPN calculator with a four-level stack, as on HP's: Return or Space pushes X; + - * / combine Y and X (Shift-= and Shift-8 on the main keys, or the keypad's); N negates, S swaps X and Y, R takes the square root, C clears; Backspace deletes a digit, or clears X. Cmd-Return types X out to the host.

**Dice.** Type a roll such as `2d6+3`, `d20` or `4d6-1`, then Return; the total shows in large digits, the single rolls under it, and the three rolls before it at the bottom. Space rolls a d20, and Return on an empty line rolls the latest roll again. Up to 20 dice of 2 to 1000 sides.

**Kana.** A drill of the 46 basic hiragana and katakana: type a kana's romaji (Hepburn; si, ti, tu, hu and o for を also count). The kana fills its box when right; when wrong, its box is framed and the answer shows. Any key goes on to the next kana, and a letter starts its answer. A kana answered wrong comes back more often, one answered right less; the weights, the counts and the longest run are kept in the settings key `roamyboard/pda/kana`, written when PDA mode ends. Tab switches between hiragana, katakana and both.

The kana come from `zmk/src/display/font_kana_40.c`: Noto Sans JP at 40 px, 1 bit per pixel, only the 92 kana, made by `zmk/tools/pda_kana_font.sh`. Noto Sans JP is under the SIL Open Font License 1.1; `font_kana_40.LICENSE` holds its copyright notice and the license.

**Tetris.** A 10 × 20 well. Left and Right (or A D, H L) move the piece, Up (W, K) or X turns it clockwise, Z anticlockwise, Down (S, J) drops it one row, Space drops it to the bottom, P pauses. The pieces come from shuffled bags of all seven; cleared lines score 100, 300, 500 or 800 times the level plus one; the level starts at 0, rises every 10 lines and speeds the fall.

**Snake.** A 17 × 32 field walled by the display. The direction keys steer; each piece of food grows the snake by two cells and speeds it up.

**Bricks.** Breakout: Left and Right move the paddle while they are held, Space serves the ball. The paddle's edges send the ball off steeper. A brick scores more the higher its row; clearing the wall builds a new one and speeds the ball up. Three balls.

**Words.** Guess the five-letter word in six tries: a filled cell is the right letter in the right place, a framed cell a letter that the word has elsewhere. Any five letters count as a guess, since the PDA has no dictionary. Under the grid, the letters not yet ruled out. The answers come from `zmk/src/pda/words.c`, which `zmk/tools/pda_words.py` generates.

**Rain.** Words fall; typing one clears it. The first letter picks the lowest word that starts with it, and a wrong letter lets the word go. A word that reaches the bottom costs one of three lives. The words fall faster and come more often as the score grows.

**Keys.** The Keys app shows a cell per key position, with keymap columns from top to bottom and rows from left to right, and a gap between the halves: filled while held, outlined once pressed, a dot before that. The line at the bottom names the latest position (`r2c13`: row 2, keymap column 13) and its HID usage in hex. It helps to check a new build's key modules. Cmd-Backspace starts over.

**Info.** The Info app shows each half's key module count, battery and build ID, and where the clock comes from: `time BT2` for the host on Bluetooth profile 2, `no CTS` when no connected host offers the time, `time err` when a read failed, `time ...` before any host was asked.

The games wait while the keys go to the host, under a "paused" banner. Their best scores (and Words' won and played counts) are kept in the settings key `roamyboard/pda/scores`, written when PDA mode ends.

## Displays

The left half shows "PDA" or "note" in place of the host name while the PDA has focus. The right half shows the PDA instead of its status screen while the left half is connected; without it, the status screen returns, with the local OTA key's hint. The PDA draws dark on light like the status screen, and inverts while it has focus, so that PDA mode cannot be mistaken for typing to the host. Its title bar shows the app and the right half's battery. `zmk/tools/screen_mock/run.sh` renders the PDA as `peripheral-pda-*`, from the shell's own drawing code.

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
| Peripheral to central | notify | Text | `81`, UTF-8 bytes; appends to the text to type out |
| | | Type out | `82`; leave PDA mode, then type the text |
| | | Leave | `83`; leave PDA mode |

Writes and notifications carry at most 20 bytes, which fits the smallest ATT MTU. A disconnect of the split link leaves PDA mode on both halves.

## Clock

The home screen's title shows the time, HH:MM, once a host has given it. `zmk/src/pda/host_time.c` on the central reads the Current Time characteristic (0x2A2B) of the Current Time Service from the connected hosts, in profile order, until one answers: whenever a host connection is encrypted, and every hour. The characteristic holds the host's local time, so the clock needs no time zone. The central keeps the time running on its uptime and sends it to the peripheral, which runs it on its own uptime and redraws each minute. Between reads, the clock drifts with the nice!nanos' 32 kHz crystals.

An iPhone serves the time; macOS does not (tried 2026-10-12). Connecting the phone for a few seconds sets the clock, and the clock keeps running when the phone goes: a later read that finds no host with the time leaves it as it is. The Info app shows which profile gave the time.

## Typing out

The central types the text as `zmk_keycode_state_changed` events, one press or release every `CONFIG_ROAMYBOARD_PDA_TYPE_OUT_STEP_MS` (10 ms by default), so a 500-byte note takes about 10 s. It assumes the host's layout is ABC - Extended, as the keymaps do: å, ä and ö go out as Option-K or Option-U and the letter. Characters with no key on that layout are skipped with a warning in the log. Entering PDA mode stops a type-out in progress.

## Storage

The peripheral keeps the note, at most 512 bytes of UTF-8, in the settings key `roamyboard/pda/note`, the games' best scores in `roamyboard/pda/scores`, and the Kana drill's weights in `roamyboard/pda/kana`. It writes both when PDA mode ends, and only when they changed.

## Latency

ZMK lets the split peripheral skip up to `CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY` (30) connection events to save power, so a message from the central waits until the peripheral listens: up to about 225 ms, about 110 ms on average. While the PDA has focus, the central asks for peripheral latency 0, so that the right half listens at every 7.5 ms connection event; when focus ends, it asks for ZMK's latency again.

## Known gaps

- Only the split build has the PDA; the unibody has no peripheral.
- Only hosts with a Current Time Service set the clock; a companion app that sets it from any host is on the backlog.
