> 💡 Future module type for the [[Modular Design (v3)]]: a column-width-or-wider module carrying a Cirque GlidePoint trackpad instead of keys. Not planned for the first build; recorded here so the interconnect keeps room for it.

## Why it fits the bus

The [[Electronics]] interconnect already carries what a Cirque pad needs: +3.3V, GND, SDA and SCL. The MCU module owns the I2C pull-ups. The one line the v3.0 pinout dropped, INT, is the pad's data-ready output; firmware can poll the pad instead, or a later interconnect revision brings INT back.

Like the terminator, a touch module has no 74HC165. PL, CLK and DATA pass straight through from the right-edge header to the left-edge socket, so the MCU's scan sees one byte fewer and the end-of-chain sentinel is unchanged.

## The part

- Cirque GlidePoint TM035035, 35 mm with curved overlay, is the pad splitkb sells inside their [Halcyon Cirque Touchpad Module](https://splitkb.com/collections/keyboard-parts/products/halcyon-cirque-touchpad-module) (€24.95 as of 2026-09-15). That module is wired for SPI on a 12-pin 0.5 mm FFC in the VIK pinout and comes with a 3D-printed mount for Halcyon keyboards. Neither the cable nor the mount helps here; buy the bare pad instead.
- Cirque's Pinnacle-based pads are sold in I2C and SPI configurations and the bus is selected by a single resistor on the pad's flex tail. Buy the I2C variant; confirm the exact part suffix against Cirque's own product sheet before ordering, and check whether a 40 mm version suits the module width better.
- The pad's flex ends in a 12-pin 0.5 mm FFC; the module PCB needs the matching connector and nothing else besides the interconnect and pass-through tracks.

## Firmware

The keyboard runs ZMK on a nice!nano. Verify that current ZMK ships an input driver for the Cirque Pinnacle over I2C before committing to the design; the MCU module's firmware then exposes the pad as a pointing device and the key-column scan is untouched.

## Open questions

- Width. A 35 mm pad does not fit a 16.5 mm key column. This is a wide module in the MCU module's class, and the 8° joint angle means the flat pad spans a curved stretch of leg. The case design decides this, after case v4.
- Placement on the leg: which finger reaches it while typing, and whether it replaces a key column or sits beside the MCU module.
- Whether to route INT on a spare interconnect pin in the next PCB revision.
