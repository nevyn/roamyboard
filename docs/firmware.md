# Firmware

roamyboard runs [ZMK](https://zmk.dev) on the nice!nano v2 in the MCU module. `zmk/` holds an out-of-tree ZMK module (a kscan driver, its devicetree binding and the shields) and the user config (`zmk/config`: west manifest, keymaps, settings). ZMK is pinned in `zmk/config/west.yml`; move the pin deliberately, since board names, bindings and Kconfig symbols change on ZMK's main branch.

| Path | What |
| --- | --- |
| `zmk/drivers/kscan/chain.c`, `chain.h` | Pure logic: sentinel search, key module count stabilizing, column mapping, key event bookkeeping. No Zephyr dependencies. |
| `zmk/drivers/kscan/kscan_165_chain.c` | Zephyr glue: SPI, /PL, polling, debouncing, the kscan callback. |
| `zmk/dts/bindings/kscan/roamyboard,kscan-165-chain.yaml` | The driver's devicetree binding; every property is documented there. |
| `zmk/dts/roamyboard.dtsi` | Devicetree shared by all shields: pins, the chain node, the 30 × 5 physical layout and matrix transform. |
| `zmk/boards/shields/roamyboard/` | The unibody shield. |
| `zmk/boards/shields/roamyboard_split/` | The `roamyboard_left` and `roamyboard_right` shields. |
| `zmk/config/roamyboard.keymap`, `roamyboard_split.keymap` | Placeholder keymaps (below). |
| `zmk/tests/` | Host tests for the pure logic: `zmk/tests/run.sh`. |

## Reading the chain

Each key module has one 74HC165. Its inputs A to E are SW1 (the bottom key, at the SW1 end) to SW5 (the top key); F, G and H are tied to GND. The 165s of a half form the **chain**: each 165's QH feeds the SER input of the next 165 toward the MCU module, and the MCU module reads QH of the nearest key module on DATA.

Every scan:

1. The driver pulses /PL low for 1 µs. All 165s load their switch states, and QH shows input H of the nearest key module.
2. The driver reads `max-key-modules + 1` bytes (33 by default) over SPIM1 at 1 MHz, MSB first. Each byte is one key module, nearest first:

   | Bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
   | --- | --- | --- | --- | --- | --- | --- | --- | --- |
   | Input | H | G | F | E | D | C | B | A |
   | Key | always 0 | SW7 | SW6 | SW5 | SW4 | SW3 | SW2 | SW1 |
   | Row | | 0 (7-key) | 1 (7-key) | 0 | 1 | 2 | 3 | 4 |

3. The first byte with bit 7 set is the **sentinel**: the terminator module ties the far end's DATA to +3.3V, so the bytes after the last key module read 0xFF. The sentinel's index is the key module count. No key modules means the first byte is the sentinel. DATA has the nRF52840's pull-up, so an MCU module with nothing attached also reads as zero key modules.

Row 0 is the top key. The `rows` property (default 5) sets how many inputs each key module uses, so a 7-key module needs only `rows = <7>`.

The SPI bus runs in mode 2 (`spi-cpol`): SCK idles high, SPIM1 samples DATA on each falling edge, and the 165s shift on each rising edge, so every sample lands half a clock period after the last shift. The first falling edge samples input H of the nearest key module, which is on QH straight after /PL. Mode 0 would sample on the same rising edge that shifts, and then depend on the 165's propagation delay, which the datasheet gives no minimum for.

## Key module count

Key modules are hot-pluggable, so the driver counts them on every scan, never only at boot. A new count is accepted only after `stable-scans` (default 3) consecutive scans observe it. While the observed count differs from the accepted one, the driver reports no key events; a key module that is sliding on or off makes the count bounce, and the keys of a half-connected chain read garbage.

When a new count is accepted, the driver first releases every key that it reports pressed and resets its debouncers, and then maps the key modules to keymap columns again. Keys that are still held get pressed again after debouncing, at their new keymap columns.

A scan whose bytes contain no sentinel is a **fault**: a missing terminator module, a broken DATA or /PL line, or more key modules than `max-key-modules`. The driver logs it at most every 5 s with the raw bytes and the number of faulty scans since the last report. A fault is fed to the stabilizer like a count: when it persists for `stable-scans` scans, the driver releases all keys, so that a broken chain cannot leave keys stuck on the host.

The driver scans every `poll-period-ms` (10 ms) while idle, and every `debounce-scan-period-ms` (1 ms) while a key is pressed or debouncing. A settling count keeps the idle pace, so `stable-scans` spans about 30 ms. Debouncing uses ZMK's integrator debouncer per key: 1 ms to press, 10 ms to release (`zmk/dts/roamyboard.dtsi`).

## Columns and anchors

The **physical column** is a key module's position in the chain: 0 is the key module nearest the MCU module. The MCU module sits at the right end of a half and the terminator module at the left end, so physical columns count right to left.

The **keymap column** is where a key module's keys land in the 30-column keymap. Each shield's chain node owns `columns` keymap columns and has an **anchor**, the end of the half that keymap columns are counted from:

| Anchor | Keymap column of physical column p | With fewer key modules than columns |
| --- | --- | --- |
| `mcu` | columns − 1 − p | the leftmost keymap columns never fire |
| `terminator` | count − 1 − p | the rightmost keymap columns never fire |

The `terminator` anchor is the only mapping that depends on the key module count: adding a key module anywhere moves every keymap column. Key modules that map outside `0` to `columns − 1` are ignored: with the `mcu` anchor those are the ones nearest the terminator module, with the `terminator` anchor the ones nearest the MCU module.

A split roamyboard anchors both halves at the middle of the keyboard:

```
              left half, anchor mcu                            right half, anchor terminator
 terminator  [11] [12] [13] [14]  MCU module  |  terminator  [15] [16] [17] [18]  MCU module
             p=3  p=2  p=1  p=0               |              p=3  p=2  p=1  p=0
```

Keymap columns are in brackets. With five key modules on each half, the left half fills keymap columns 10 to 14 and the right half 15 to 19, wherever in the half the fifth one goes.

The kscan reports keymap columns within its half; the matrix transform's `col-offset` of 15 on the right half places them in 15 to 29.

Every build uses one physical layout, 30 × 5 keys (positions `row * 30 + column`), so ZMK Studio shows the full width and the keys of missing key modules never fire.

## Builds

`zmk/build.yaml` lists three builds, all for the nice!nano v2 (`nice_nano//zmk`, default revision 2.0.0) with the nice!view (`nice_view_adapter nice_view`):

| Build | Shield | Role | Columns | Anchor | ZMK Studio |
| --- | --- | --- | --- | --- | --- |
| `roamyboard` | `roamyboard` | unibody | 0 to 29 | `mcu` | yes |
| `roamyboard_left` | `roamyboard_left` | split central | 0 to 14 | `mcu` | yes |
| `roamyboard_right` | `roamyboard_right` | split peripheral | 15 to 29 | `terminator` | no |

The split role is fixed at compile time. ZMK Studio runs over BLE and over USB (the `studio-rpc-usb-uart` snippet), with locking off; the peripheral cannot host it.

The unibody and the split shields live in separate shield directories because ZMK picks the keymap by the shield directory's name before the shield's own name: all three would otherwise share `roamyboard.keymap`.

Deep sleep (`CONFIG_ZMK_SLEEP`) stays off: the chain is polled, so no key press can wake the nRF52840 from System OFF.

The 165s run from the nice!nano's VCC pin, which P0.13 switches. `roamyboard.dtsi` holds P0.13 high with a GPIO hog and disables ZMK's ext-power node, so that neither a keymap behavior nor a setting saved by earlier firmware can switch the chain off.

Releasing every held key queues one event per key at once, so each shield raises `CONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE` from ZMK's 4 to 32.

## Keymap

`zmk/config/roamyboard.keymap` (unibody) and `zmk/config/roamyboard_split.keymap` (both halves) are placeholders that let a socket board with a few key modules type, until the real layout is designed ([Layout](../zmk/Layout.md)). Layer 0 is a Lily58-like 6 + 6 QWERTY block (number row, QWERTY, home row, bottom row, thumb and modifier row); every other position is `&none`. On the unibody the block fills keymap columns 18 to 29, the 12 key modules nearest the MCU module; on the split it fills 9 to 20, the 6 key modules nearest the middle on each half. Either `&mo 1` key reaches layer 1: `&bt BT_SEL 0` (Mac), `1` (iPad), `2` (phone), `&bt BT_CLR`, `&studio_unlock`, `&bootloader` and `&sys_reset`.

## Pins

The firmware chooses the pins; the MCU board does not exist yet. Until it does, the MCU module takes the socket board, wired to the nice!nano as below. The interconnect pins are the socket board's J_LEFT sockets (pinout in [Electronics](../electronics/Electronics.md)); pin 1 is the topmost pin of each connector.

| Signal | nice!nano pin | nRF52840 | Socket board |
| --- | --- | --- | --- |
| +3.3V | VCC | switched by P0.13 | J_LEFT1 pin 2 |
| GND | GND | | J_LEFT1 pin 3 |
| CLK | D15 | P1.13, SPIM1 SCK | J_LEFT2 pin 1 |
| /PL | D18 | P1.15, GPIO | J_LEFT2 pin 2 |
| DATA | D14 | P1.11, SPIM1 MISO, pull-up | J_LEFT2 pin 3 |

+5V (J_LEFT1 pin 1) and the bottom connector (LED, SDA, SCL) stay unconnected for now.

The nice!view, as ZMK's `nice_view_adapter` maps it on the nice!nano:

| nice!view | nice!nano pin | nRF52840 |
| --- | --- | --- |
| VCC | VCC | switched by P0.13 |
| GND | GND | |
| SCK | D3 | P0.20, SPIM0 SCK |
| MOSI | D2 | P0.17, SPIM0 MOSI |
| CS | D1 | P0.06, active high |

Why these pins:

- SPIM1 is the nice!nano's `pro_micro_spi` instance, with SCK on D15 and MISO on D14 in the board's own pin control. Its MOSI (D16, P0.10) is left unconnected.
- spi0 belongs to the nice!view (SCK D3/P0.20, MOSI D2/P0.17, CS D1/P0.06) and shares its peripheral with i2c0.
- D18 sits next to D15 on the header row that also carries VCC and GND, so all five wires leave from one edge of the nice!nano. It is not an NFC pin (D10 and D16 are) and not a UART pin (D0 and D1).

## Building

GitHub Actions (`.github/workflows/build.yml`) builds all three UF2s on every push that touches `zmk/`, and runs the host tests. ZMK's reusable `build-user-config` workflow cannot build this repo: it expects the config and `zephyr/module.yml` at the repo root, and it checks ZMK out into `./zmk`, which is this repo's module. The workflow does the same steps with the right paths. Download the UF2s from the run's artifacts.

Local build with Docker, from the repo root. The west workspace lives outside the repo and is reused between builds:

```sh
ws=${WS:-$(mktemp -d /tmp/roamyboard-west.XXXXXX)}
mkdir -p "$ws/config" && cp zmk/config/west.yml "$ws/config/"
docker run --rm -v "$ws:/west" -v "$PWD:/repo:ro" -w /west zmkfirmware/zmk-build-arm:4.1 sh -c '
  [ -d .west ] || { west init -l config && west update --fetch-opt=--filter=tree:0; }
  west zephyr-export
  west build -p -s zmk/app -d build/roamyboard -b nice_nano//zmk -S studio-rpc-usb-uart -- \
    -DZMK_CONFIG=/repo/zmk/config -DZMK_EXTRA_MODULES=/repo/zmk \
    -DSHIELD="roamyboard nice_view_adapter nice_view" \
    -DCONFIG_ZMK_STUDIO=y -DCONFIG_ZMK_STUDIO_LOCKING=n'
ls -l "$ws/build/roamyboard/zephyr/zmk.uf2"
```

For the split halves use `-d build/roamyboard_left` with `SHIELD="roamyboard_left nice_view_adapter nice_view"` (same snippet and Studio flags), and `-d build/roamyboard_right` with `SHIELD="roamyboard_right nice_view_adapter nice_view"` without `-S studio-rpc-usb-uart` and the Studio flags. For USB logs, add the `zmk-usb-logging` snippet (`-S zmk-usb-logging`) and read the nice!nano's serial port.

The host tests need only a C compiler: `zmk/tests/run.sh`.

## Flashing

1. Connect the nice!nano over USB and double-tap its reset button (or use `&bootloader` on layer 1). It mounts as a USB drive named NICENANO.
2. Copy the UF2 for that nice!nano onto the drive. The nice!nano flashes it and restarts.

For a split, flash `roamyboard_left` onto the left half and `roamyboard_right` onto the right. If the halves do not find each other after switching from other firmware, flash ZMK's `settings_reset` firmware onto both, then the roamyboard firmware again.
