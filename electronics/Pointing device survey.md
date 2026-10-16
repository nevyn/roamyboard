> 💡 Candidate trackpads and trackballs for a pointing module, compared against the roamyboard's interconnect, module size and battery. Surveyed 2026-10-16; prices and stock are as seen that day. Every figure comes from the linked source; a blank cell means no primary source was found. The earlier sketch is [[Touch module]].

## Constraints

- **Size.** A key module is about 21 mm wide at the column pitch and 100 mm long. A flat module N columns wide keeps the half's curve when its side faces meet at N × 8°; a 42 mm flat top deviates about 1.5 mm from the arc it replaces (149 mm radius), a 62 mm one about 3.3 mm. The MCU module is 52.4 mm wide. A pointing module is therefore 42 mm wide at most in the middle of a half, about 55 mm at an end.
- **Depth.** The modules are shallow: the MCU module's bay holds a 5 mm battery.
- **Lines.** The bottom connector's LED, SDA and SCL lines pass through every key module unused, and the top connector's +5V pin becomes INT. A pointing module should use SDA, SCL and INT only, so that LED stays free for key LEDs; taking LED as well gives four lines, enough for 3-wire SPI plus MOTION.
- **MCU.** The nRF52840 shares TWIM0 with SPIM0 (the nice!view) and TWIM1 with SPIM1 (the chain), so an I2C device needs the chain moved to SPIM2. TWIM runs at 400 kHz at most.
- **Power.** One 750 mAh LiPo feeds everything, and the keyboard never deep-sleeps.
- **Latency.** The Mac's Bluetooth connection interval is about 15 ms, so report rates above 100 Hz gain little. What matters is the latency of the first touch out of the controller's low-power mode.

## Trackpads

### Finished modules

| | Azoteq TPS48-P203-S | Azoteq TPS43-201A-S | Azoteq TPS65-201A-S | Cirque TM035035 / TM040040 |
| --- | --- | --- | --- | --- |
| Outline | 48 × 38 mm [A] | 43.30 × 40.30 mm [B] | 65 × 49 mm [B] | 35 / 40 mm round [E] |
| Thickness | 2.03 mm with adhesive [A] | 2.03 mm, 2.23 mm with Mylar [B] | as TPS43 [B] | TM040040 "40 mm x 4.1 mm" (Mouser search snippet, unverified) |
| Controller | IQS572, 1792 × 1792 [A] | IQS572, 2048 × 1792 [B] | IQS550, 3072 × 2048 [B] | Pinnacle 1CA027 |
| Touch | 5 fingers; tap, press and hold, swipe, two-finger tap, scroll, zoom on chip [C] | same | same | 1 finger |
| Report rate | IQS572 135 Hz typ [C]; "170Hz scan rate for gesture recognition" [A] | 135 Hz [C] | | 100 SPS default [Q] |
| Current | tracking 4 mA typ, low power 35 µA typ [A] | "TBD" [B] | "TBD" [B] | 23 mm pad: active 11.6 mA, idle 1.9 mA, sleep 870 µA [E] |
| Interface | I2C 0x74, 400 kHz, clock stretching, RDY active high [C] | same; FPC carries RDY, SDA, GND, VDDHI, SCL, NRST [B] | same | I2C 0x2A or SPI; HW_DR |
| Price | $2.73, 59 in stock, DigiKey | $5.29, 214 in stock, DigiKey | | TM035035-2024-003 €8.27 (Mouser search snippet, unverified) |

- Only the P203 suffix is I2C; P201 parts signal gestures on GPIO pins only [A]. The 40 mm round TPR40-P103 is obsolete.
- The IQS5xx keeps its communication window open after an I2C STOP until the host writes to 0xEEEE. If the host starts a transfer while the chip is in a low-power mode, the chip NACKs the first address; the host retries after at least 150 µs [C].
- IQS572 current depends on the report interval: 2.73 mA at 10 ms, 1.85 mA at 15 ms, 346 µA at 80 ms, 174 µA at 160 ms [C]. Each power mode (Active, Idle-Touch, Idle, LP1, LP2) has its own interval, so the first-touch latency is the interval that the firmware sets.
- ZMK drivers for the IQS5xx are community modules: [AYM1607/zmk-driver-azoteq-iqs5xx](https://github.com/AYM1607/zmk-driver-azoteq-iqs5xx), tested on TPS43 and TPS65, and [stelmakhdigital/zmk_driver_azoteq](https://github.com/stelmakhdigital/zmk_driver_azoteq). QMK has `azoteq_iqs5xx`. Zephyr has none.
- Cirque's pads ship in SPI mode; I2C needs resistor R1 removed (tm040040 crate README, a forum anecdote). Pads with firmware ID 0x0E, version 0x75 ("rev2025") cap the sample rate at 100 and ignore ADC gain, calibration matrix and edge tuning [P2]. Waking from sleep "take[s] about 300 milliseconds" [P]. Zephyr's `cirque,pinnacle` driver needs the data-ready line, probes once at boot and calibrates for about 200 ms.
- No finished narrow-strip 2D trackpad exists. Laptop precision touchpads start at 105 × 65 mm (Cirque TM105065 [F]), and Zephyr has no I2C-HID host.

### Controllers for a custom sensor on the module's PCB

The module's own board can carry the electrodes, under a thin overlay, so that the pad takes the module's shape. At Azoteq's 5 mm electrode pitch ([AZD128](https://www.azoteq.com/images/stories/pdf/azd128-gamepad-trackpad-design-guide_v1.0.pdf), Table 2.4), a 40 × 90 mm pad needs 8 × 18 = 26 electrodes.

| IC | Electrodes | Largest pad | Current | Package | Source |
| --- | --- | --- | --- | --- | --- |
| Azoteq IQS9151 | 13 Rx, 22 Tx, 25 electrodes | about 85 × 40 mm (17 Tx × 8 Rx) | active 2.2 to 9.5 mA, LP2 4 to 38 µA | QFN52 6 × 6, 0.40 mm pitch; not at LCSC | [datasheet](https://www.azoteq.com/images/stories/pdf/IQS9150_IQS9151_datasheet.pdf) |
| Azoteq IQS9150 | 26 Rx, 22 Tx, 45 electrodes | about 130 × 95 mm | as IQS9151 | QFN52; DigiKey $2.07 | same |
| Azoteq IQS550 | 15 × 10 | about 90 × 60 mm at 6 mm pitch | 2.52 mA at 15 ms | QFN48 7 × 7; LCSC C5271144, out of stock | [datasheet](https://www.azoteq.com/images/stories/pdf/iqs5xx-b000_trackpad_datasheet.pdf) |
| Azoteq IQS7211E | 13 electrodes | about 40 × 25 mm | 985 µA at 16 ms, LP 5 to 11 µA | QFN20 3 × 3; "Not For New Designs" at DigiKey | [datasheet](https://www.azoteq.com/images/stories/pdf/IQS7211E_Datasheet_v1.5.pdf) |
| Microchip MTCH6102 | 15 channels, self capacitance | 40 × 90 mm only with 10 mm diamonds | | SSOP28 / UQFN28; LCSC C637766 | [datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/40001750A.pdf) |
| Cirque Pinnacle 1CA027 | 16 × 12 | | idle 2.0 mA | 48-TQFP; Cirque sales only | [Cirque](https://www.cirque.com/gen2gen3-asic-details) |

- The IQS9150 and IQS9151 take their whole configuration over I2C at boot, from a header file that Azoteq's Windows GUI exports. No driver exists for ZMK, Zephyr or Linux.
- Azoteq lists an application note on curved touchpads for the IQS550, "AZD076 - How to design a non-uniform/curved touchpad" ([product page](https://www.azoteq.com/product/iqs550/)); it was not read.
- IQS7211E pads have ZMK drivers ([sekigon-gonnoc](https://github.com/sekigon-gonnoc/zmk-driver-iqs7211e), [amgskobo](https://github.com/amgskobo/zmk-iqs7211e-driver)), but 13 electrodes limit them to a small pad.

## Trackballs

| Sensor | Lines | CPI | Speed | Supply | Current | Driver |
| --- | --- | --- | --- | --- | --- | --- |
| PixArt PMW3610 | 3-wire SPI (SDIO, SCLK, NCS) + MOTION; SCLK ≤ 2 MHz | up to 3200 | 30 ips | VDD 1.7 to 2.1 V, VDDIO up to 3.3 V | run 0.60 mA; rest 36, 16, 7 µA; shutdown 3 µA | Zephyr upstream `pixart,pmw3610`; [badjeff/zmk-pmw3610-driver](https://github.com/badjeff/zmk-pmw3610-driver) |
| PixArt PAW3395 | 4-wire SPI + MOTION | up to 26000 | 650 ips | VDD 1.8 to 2.1 V | run 1.3 to 1.7 mA; rest down to 6 µA | [badjeff/zmk-paw3395-driver](https://github.com/badjeff/zmk-paw3395-driver) |
| PixArt PAW3222 | SPI + motion | 608 to 4826 (Zephyr) | | | | Zephyr upstream `input_paw32xx.c` |
| PixArt PMW3389 | 4-wire SPI + MOTION | up to 16000 | 400 ips | VDD 1.8 to 2.1 V | run 21 mA | community |

Sources: [PMW3610 datasheet](https://atta.szlcsc.com/upload/public/pdf/source/20250805/5683F7E84178FEFE1E76BD4C4B33C995.pdf), [PAW3395 datasheet](https://payda.krakow.pl/mpattachment/file/download/id/1236), [PMW3389 datasheet](https://payda.krakow.pl/mpattachment/file/download/id/557). No PAW3222 datasheet was found.

- The PMW3610 needs a 1.8 V regulator on the module. Its rest-to-run wake latency and frame rate were not found.
- The PAW3395 needs five lines, one more than the interconnect has.
- The tracked surface (the ball's bottom) sits 9.81 mm below the top of a PMW3389 or PAW3395 chip; the PMW3610's full stack height was not found.
- The shallowest wireless ZMK trackball found is the Keychron Nape Pro: PAW3222, 25 mm ball, 19 mm thick and 36 mm tall with the ball ([spec sheet](https://www.gizmodo.jp/article/gizmart-nape-pro/)). A 25 mm ball therefore stands well above a roamyboard module's top.
- Static zirconia bearings rank above ball transfer units in a [community comparison](https://kbd.news/Trackball-bearing-comparison-1757.html); their stiction suits use while walking.
- Mini modules (Pimoroni PIM447 Hall-effect trackball, BlackBerry trackballs, optical finger navigation sensors such as the PAT9125 and ADBS-A350) are either discontinued, too coarse, or limited to 20 to 30 ips.

## Shortlist

1. **Azoteq TPS48-P203-S.** It fits a 42 mm module across its 38 mm side, is 2 mm thick, tracks five fingers with scrolling on chip, and costs $2.73. It needs SDA, SCL and RDY on INT; its NRST can stay pulled up on the module (whether a reset over I2C can replace it is unverified), so LED stays free. The ZMK drivers are community modules, tested on the TPS43 and TPS65 but not on the TPS48.
2. **Custom PCB sensor with an IQS9151.** It is the only route to a pad as long as the column (about 85 × 40 mm). It needs a sensor layout, tuning in Azoteq's Windows GUI, a 0.4 mm-pitch QFN assembled at JLCPCB from consigned parts, and a driver of our own.
3. **PMW3610 with a 25 mm ball.** Its 0.60 mA fits best among the trackballs and Zephyr ships its driver, but its four lines take LED too. The ball stands well above the module and the 30 ips ceiling leans on pointer acceleration.

The Cirque pads fall behind the TPS48: one finger, about twice the thickness (unverified), a round outline that wastes the column's length, and either about 300 ms to wake from sleep or about 2 mA idle.

## Sources

- [A] [ProxSense GPIO trackpad datasheet](https://www.azoteq.com/images/stories/pdf/proxsense_gpio_trackpad_datasheet.pdf), v1.09
- [B] [TPS43, TPS65 datasheet](https://media.digikey.com/pdf/Data%20Sheets/Azoteq%20PDFs/TPS43,_TPS65_Datasheet_Web.pdf), v1.02
- [C] [IQS5xx-B000 trackpad datasheet](https://www.azoteq.com/images/stories/pdf/iqs5xx-b000_trackpad_datasheet.pdf), rev 2.1
- [E] [Cirque GlidePoint circle trackpads](https://cirque.com/glidepoint-circle-trackpads)
- [F] [Cirque GlidePoint specifications](https://www.cirque.com/glidepointspecification)
- [P] [CircuitPython Cirque Pinnacle driver](https://circuitpython-cirque-pinnacle.readthedocs.io/en/latest/_modules/circuitpython_cirque_pinnacle.html)
- [P2] [PinnacleTouch.isRev2025](https://cirquepinnacle.readthedocs.io/en/latest/API/cpp-generated/PinnacleTouch.isRev2025.html)
- [Q] [QMK pointing device docs](https://docs.qmk.fm/features/pointing_device)
