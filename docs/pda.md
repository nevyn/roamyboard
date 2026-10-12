# Micro PDA

The micro PDA runs on the split peripheral (the right half) and uses its nice!view. While PDA mode is on, the central (the left half) sends its keyboard keys to the PDA instead of the host. Today the PDA has one app, a note that can be typed out to the host.

## Keys

Both keys sit on the System layer of `zmk/config/roamyboard_split.keymap`:

- **PDA** (`&pda_sel`, the 6 key) selects the PDA as slot 6, a sixth device beside the five Bluetooth profiles.
- **Note** (`&pda_note`, the 7 key) opens a quick note.

Pressing the key of the mode that is on returns the keys to the host. Choosing another Bluetooth profile or output (`&bt BT_SEL`, `&out`) also returns them, as choosing another device would. A host that connects or disconnects meanwhile does not, and neither does selecting the profile that is already chosen.

In the note:

| Key | Does |
|---|---|
| Letters, digits, punctuation, Space, Tab | Type themselves, with Shift. |
| Return, keypad Enter | Start a new line. |
| Backspace | Deletes the last character. |
| Cmd-Backspace | Deletes the whole note. |
| Cmd-Return | Returns the keys to the host and types the note out to it, then empties the note. |
| Escape | Returns the keys to the host. The note stays. |
| å, ä, ö (Keypad layer) | Type themselves: the PDA reads the keymap's Option dead keys back into the letters. |

Consumer keys, such as volume and media keys, still reach the host in PDA mode.

## Displays

The left half shows "PDA" or "note" in place of the host name. The right half replaces its status screen with the PDA view, inverted (white on black), so that PDA mode cannot be mistaken for typing to the host. The view shows the mode, a rule, and the end of the note with a cursor; older lines scroll off the top. `zmk/tools/screen_mock/run.sh` renders it as `peripheral-pda-*`.

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
| Peripheral to central | notify | Text | `81`, UTF-8 bytes; appends to the text to type out |
| | | Type out | `82`; leave PDA mode, then type the text |
| | | Leave | `83`; leave PDA mode |

Notifications carry at most 20 bytes, which fits the smallest ATT MTU. A disconnect of the split link leaves PDA mode on both halves.

## Typing out

The central types the text as `zmk_keycode_state_changed` events, one press or release every `CONFIG_ROAMYBOARD_PDA_TYPE_OUT_STEP_MS` (10 ms by default), so a 500-byte note takes about 10 s. It assumes the host's layout is ABC - Extended, as the keymaps do: å, ä and ö go out as Option-K or Option-U and the letter. Characters with no key on that layout are skipped with a warning in the log. Entering PDA mode stops a type-out in progress.

## Storage

The peripheral keeps the note, at most 512 bytes of UTF-8, in the settings key `roamyboard/pda/note`, and writes it when PDA mode ends.

## Known gaps

- Only the split build has the PDA; the unibody has no peripheral.
