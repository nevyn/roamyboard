> 💡 A pointing module is a module of the [[Modular Design (v3)]] that carries a trackpad instead of keys. The candidates are compared in [[Pointing device survey]]. Decided 2026-10-18.

## Status

The firmware runs the pointing module's I2C bus, TWIM1 on D4 and D5, and the chain on SPIM2 since build ca87098. The MCU modules are not rewired yet, so SDA, SCL, INT and the pull-ups below are the plan, and no pad is built.

## Decisions

- **Trackpad first.** The pad is the Azoteq TPS48-P203-S: an IQS572 on a 48 × 38 mm board, 2 mm thick, I2C at 0x74, up to five fingers with scroll and tap gestures on the chip. The TPS43-201A-S is the bench reference, since the community ZMK drivers were tested on it. A PMW3610 trackball stays the alternative. A pointing stick is out: it controls the pointer's speed, not its position, so long moves are slow.
- **Width.** At most two columns, about 42 mm, with side faces 16° apart, so the half's curve is kept. The TPS48's 38 mm side runs across the column.
- **Interconnect.** J_LEFT1 and J_RIGHT1 pin 1, +5V in the v3.0 pinout, becomes INT: the pad's data-ready output (RDY on the IQS5xx), active high. LED stays reserved for key LEDs. The pointing module uses SDA, SCL and INT only, and pulls the pad's NRST up on its own board. Key module boards need no change, since they pass pin 1 straight through on its own track.
- **MCU module.** SDA on D4 (P0.22), SCL on D5 (P0.24), INT on D6 (P1.00), next to the nice!view's wires on the same edge of the nice!nano. 2.2 kΩ through-hole pull-ups from SDA and SCL to VCC. The free pins on the chain's edge, D19 to D21 (P0.02, P0.29, P0.31), are "low frequency I/O only" in the nRF52840 product specification, so they are not used for I2C.
- **I2C peripheral.** The chain runs on SPIM2 instead of SPIM1, so that TWIM1 is free; TWIM0 shares its hardware with SPIM0, which drives the nice!view. The bus runs at 400 kHz, TWIM's maximum.
- **Hot-plug.** The firmware probes 0x74 while the pad is absent and configures the pad whenever it appears, so a pointing module can slide on after power-up. The pad calibrates when it starts, so a finger on it during slide-on gives a bad baseline until the next start.
- **Latency and power.** The pad keeps a short report interval while ZMK is active and drops to its low-power mode only while ZMK is idle, so the first touch while typing is reported within one interval. The IQS572 draws 1.85 mA at a 15 ms interval and 35 µA in low power.
- **Chain.** The pointing module has no 74HC165: CLK, /PL and DATA pass straight through, so the chain reads one key module fewer.

## Open questions

- Which half and where in it, and which finger reaches the pad. The left half, the split central, saves the split hop.
- Clicking: the pad's tap gesture, thumb keys bound to mouse buttons, or buttons on the module, read by a 74HC165 like a key module's keys.
- The overlay. The TPS48 takes a 1 to 2 mm or 2 to 3 mm overlay, chosen by solder link S1, so the printed shell top may serve as the overlay.
