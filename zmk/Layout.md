# Layout for roamyboard

The layout carries Nevyn's Naya Create layout over to the roamyboard: five layers, 7 key modules per half. `zmk/config/roamyboard_split.keymap` and `roamyboard.keymap` (unibody) implement it; edits made in ZMK Studio are stored on the keyboard and override these files.

The tables show the left half, then the right half, as they lie on the legs. On both halves, columns are counted from the outer edge: column 1 is the pinky's outer column and column 7 the index finger's inner column. Row 1 is the SW5 end (number row), row 5 the SW1 end.

- **Thumb keys** are row 5 of the three inner columns (columns 5 to 7): fn⌫/L1 ␣ ⌫ on the left and ⌫ ␣ fn⌫/L2 on the right, so both thumbs have space and backspace. The thumbs reach row 5, so the SW1 end of every key module faces the hip.
- **Layer keys**: the thumbs are the main way into the layers. Each thumb's fn⌫ is a layer-tap: a tap sends forward delete, and holding it is L1 on the left thumb and L2 on the right. It is decided as a hold as soon as another key goes down (flavor hold-preferred), and a release after the 200 ms tapping term sends nothing, so a thumb that holds and then lets go never deletes. With a thumb on the layer, the hand stays on its home keys: the arrows sit on E S D F and volume and media on W E R and S D F. The pinkies still hold the layers too: L1 and L2 in the left pinky's column 1, and L1 in the right pinky's column 1. A pinky that holds a layer key cannot press the other keys of its column, so the held key is transparent on its layer, and the left pinky's column carries nothing else on layer 1 and only Pwr, Boot, OTA and To3 on layer 2. To reach the right pinky's column on layer 1, hold L1 with the left pinky.
- The thumb keys are transparent on layers 1 to 3, except for keypad 0 and the decimal point on the right thumbs.
- å, ä and ö on the Keypad layer (å and ä where [ and ] are on QWERTY, ö next to the keypad minus) type Swedish letters while the Mac uses a US input source, so code keeps its US layout. Each one types the Mac's Option dead-key sequence: Option-K then A for å, Option-U then A for ä, Option-U then O for ö; with Shift held, the letter is the capital. The sequences are for the ABC – Extended input source, where Option-K is the ring; on plain ABC and U.S., Option-K types ˚ instead, so å comes out wrong there. A mod-morph picks the capital and hides the held Shift from the Option keystroke, and each macro taps its keys 10 ms apart so it finishes before a quick release of the key clears that mask. On any non-US input source, such as Swedish, these keys type the wrong characters.
- ⇧⌃ is Shift + Control in one key. fn⌫ is forward delete.
- System: BT1 to BT5 select Bluetooth profiles 0 to 4, all five that ZMK keeps; BT× clears the selected profile's pairing; BLE and USB choose the output. Pwr turns the keyboard off (ZMK soft off); only the reset button turns it back on, because the polled chain cannot wake the nice!nano. Pwr, Boot, OTA and To3 sit in the left pinky's column, which the pinky that holds L2 cannot press, so another finger has to reach over for them, and pressing one by accident is less likely. To3 is where To0 is on the Gaming layer, so one key position goes into and out of Gaming. Boot shows the bootloader view on the nice!view and then enters the UF2 bootloader, on the half that it is pressed on, so each half has one. OTA, next to Boot on each half, does the same for the bootloader's Bluetooth mode, which takes a DFU zip from a Mac or a phone ([Firmware](../docs/firmware.md), Updating over Bluetooth). Rst restarts the firmware.
- PDA, on the System layer's 6 key next to BT5, sends the keys to the micro PDA on the right half's display instead of the host, as a sixth device; pressing it again, or choosing a Bluetooth profile or output, gives the keys back to the host. Note, on the 7 key, opens the PDA's note editor directly; Escape returns to the host ([PDA](../docs/pda.md)).
- The System layer passes the modifiers through: ⇧, ⌃, ⌥ and ⌘ keep their QWERTY positions (on the left half, ⌃ only in row 5), so they combine with the mouse keys, for example ⌘ + click or ⇧ + scroll.
- Mouse keys on the System layer: the right hand moves the pointer with I (up) and J K L (left, down, right) and scrolls with Y (up) and H (down). The left thumb clicks: M1 (left click) on ␣, M2 (right click) on ⌫ and M3 (middle click) on fn⌫. The right thumb holds L2 meanwhile, and the right hand's fingers move the pointer; the right thumb's M1 and M2 still click when L2 is held with the left pinky.
- ↩/Boot on the Keypad layer is ↩ when tapped and Boot when held for 1.5 s. It is the outer column of the right half, the key module nearest the MCU module on a unibody, so a unibody with a single key module reaches the bootloader by holding L1 and then ↩/Boot.
- To3 switches to the Gaming layer and To0 back to QWERTY.
- To4, on the System layer next to To3, locks the keyboard: on the Locked layer every key does nothing, except the left half's row 5 Control key, which returns to QWERTY when held for 1 s. The keyboard stays on and connected, and the status screen shows "Locked". Locking takes the left row 5 Control off the System layer; the right half's Control still passes through.
- 0 on the right half (row 1, column 3), where OTA sits on the System layer, is also the right half's local OTA key: while the right half has no connection to the left half, holding it for 3 s enters OTA mode on the right half, so that the right half can be updated while the left half is down ([Firmware](../docs/firmware.md), Updating over Bluetooth). While the halves are connected, it does only what the keymap binds to it.

Legend: ︶ is transparent (the layer below decides), ⊘ does nothing, an empty cell is unassigned (also does nothing).

## 0 QWERTY

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Esc | \` | 1 | 2 | 3 | 4 | 5 |   | 6 | 7 | 8 | 9 | 0 | - | = |
| ⇧⌃ | ⇥ | Q | W | E | R | T |   | Y | U | I | O | P | [ | ] |
| L1 | ⌃ | A | S | D | F | G |   | H | J | K | L | ; | ' | \\ |
| L2 | ⇧ | Z | X | C | V | B |   | N | M | , | . | / | ⇧ | L1 |
| ↩ | ⌃ | ⌥ | ⌘ | fn⌫/L1 | ␣ | ⌫ |   | ⌫ | ␣ | fn⌫/L2 | ⌘ | ⌥ | ⌃ | ↩ |

## 1 Keypad and arrows

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|   |   | F1 | F2 | F3 | F4 | F5 |   | F6 | F7 | F8 | F9 | F10 | F11 | F12 |
| ︶ |   | Ins | Hm | ↑ | End | PgU |   | × | 7 | 8 | 9 | + | å | ä |
| ︶ |   | fn⌫ | ← | ↓ | → | PgD |   | . | 4 | 5 | 6 | − | ö | ⇥ |
| ︶ | ⇧ |   |   |   |   |   |   | , | 1 | 2 | 3 | ÷ | ︶ | ︶ |
| ︶ | ⌃ | ⌥ | ⌘ | ︶ | ︶ | ︶ |   | ︶ | 0 | . | ︶ | ︶ | ︶ | ↩/Boot |

## 2 System

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Pwr | BT× | BT1 | BT2 | BT3 | BT4 | BT5 |   | PDA | Note | ⊘ | ⊘ | OTA | Boot | Rst |
| Boot | BLE | ⊘ | V− | Mut | V+ | ⊘ |   | W↑ | ⊘ | M↑ | ⊘ | ⊘ | ⊘ | ⊘ |
| OTA | USB | ⊘ | Prv | Ply | Nxt | ⊘ |   | W↓ | M← | M↓ | M→ | ⊘ | ⊘ | ⊘ |
| ︶ | ︶ | ⊘ | Br− | ⊘ | Br+ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ︶ | ⊘ |
| To3 | To4 | ︶ | ︶ | M3 | M1 | M2 |   | M2 | M1 | ︶ | ︶ | ︶ | ︶ | ⊘ |

## 3 Gaming

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Esc | \` | 1 | 2 | 3 | 4 | 5 |   | 6 | 7 | 8 | 9 | 0 | - | = |
| ︶ | ⇥ | Q | W | E | R | T |   | Y | U | I | O | P | [ | ] |
| ︶ | Cps | A | S | D | F | G |   | H | J | K | L | ; | ' | \\ |
| ︶ | ⇧ | Z | X | C | V | B |   | N | M | , | . | / | ↑ | PrS |
| To0 | ⌃ | ⌥ | ⌘ | ︶ | ︶ | ︶ |   | ︶ | ︶ | ︶ | ↩ | ← | ↓ | → |

## 4 Locked

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |
| ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |
| ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |
| ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |
| ⊘ | Unlock (hold 1 s) | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |   | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |

# Other layouts
Here's Lily58's layout:
![[Default layout.png]]

[Nevyn's old Gergo](https://github.com/nevyn/qmk_firmware/blob/nevyn/keyboards/gergo/keymaps/nevyn/keymap.c) layout looks like this:
![[nevgergo0.png]]

![[nevgergo1.png]]
![[nevgergo2.png]]
![[nevgergo3.png]]

A "Nav" layer looks pretty nice:

![[Totem nav layout.png]]