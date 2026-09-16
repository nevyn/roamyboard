> 💡 This document describes the electrical wiring of the [[Modular Design (v3)]]. 

> ✅ **Design status:** Validated on a hand-wired single-column prototype. The shift-register chain, the terminator sentinel trick, and the M5StickC-based test firmware are all confirmed working.
![[prototype column.png]]
## Concepts
### Column modules and interconnects

Each column is its own module. There are three kinds of modules: MCU, Key and Terminator. The MCU is the rightmost, terminator the leftmost, and an arbitrary number of key modules in between. A fourth kind, a trackpad column, is sketched in [[Touch module]] for later.

The interconnect between each module has 9 signals total:
- Power: +5V, +3.3V, GND
- Key input: /PL, CLK and DATA for the shift register
- NeoPixel data line, to control RGB LEDs
- Future proofing: three pins for I2C (SDA, SCL and INT). Useful if we want touch pad, etc

The 9 signals are distributed across **three 1×3 connector pairs** — top, middle, and bottom of the board. This approach was chosen over a single 9-pin connector because:
- Distributing the connection across three points along the board edge provides better mechanical stability between modules than a single point would
- Matching 9-pin pogo-pin connectors were prohibitively expensive (~$10 per pin header, 12+ per keyboard)
- Three 1×3 connectors fit within the keyswitch courtyard constraints on the PCB, whereas a single 1×9 did not

Connectors used: **hanxia HX PZ2.54-1x3P WT** pin header (male, LCSC C46061676) on the right edge, mating with the **hanxia HX PM2.54-1x3P WT** socket (female, LCSC C46061767) on the left edge. Both are 1×3, 2.54 mm pitch, horizontal SMT, mounted on the back of the board and laid flat so the pins and the socket mouth protrude past the board edge and modules click into each other straight-on. Body 8.5 × 2.5 mm on the socket, 2.5 × 2.5 mm plus 6 mm pins on the header, pin axis 1.25 mm above the board. The same footprints fit the Harwin M20-791 / M20-889 pair ([DRG-02613](https://content.harwin.com/m/0e0398fdb977d498/original/DRG-02613-Technical-Drawing-Datasheet-M20-791R-pdf.pdf), [DRG-02615](https://content.harwin.com/asset/bcd8efee-7ad9-4ddd-8fcc-2925970fdfe6/DRG-02615-Technical-Drawing-Datasheet-M20-889-pdf.pdf)), which have the same bodies and pins with thicker gold and a published 300-cycle life at ten times the price; the footprints are drawn from the Harwin drawings and carry their names. Because adjacent modules meet at the case's 8° joint angle, the pins are bent 8° after soldering so they enter the next module's sockets along its board plane.
![[module-joint.svg]]

v3.0 pinout. J_LEFT*n* are the sockets, J_RIGHT*n* the pin headers; pin 1 is the topmost pin of each connector and the same signal sits on the same pin on both edges, so every signal passes straight through the module:

| Connector | Pin 1 | Pin 2 | Pin 3 |
| --------- | ----- | ----- | ----- |
| J_LEFT1 / J_RIGHT1 (top) | +5V | +3.3V | GND |
| J_LEFT2 / J_RIGHT2 (middle) | CLK | /PL | DATA |
| J_LEFT3 / J_RIGHT3 (bottom) | LED | SDA | SCL |

DATA is the one signal that does not pass straight through: J_LEFT2 pin 3 is the 165's serial input and J_RIGHT2 pin 3 its serial output. It sits on the bottom pin because the two pass-through tracks above it would block every other way off the pad. INT was dropped; 9 pins is all three 1×3 connectors carry.
### MCU module
The MCU module contains a nice!nano 2.0, battery, USB-C for wired connection and charging, and power switch. Possibly also an OLED display, and maybe a rotary encoder for settings (pairing, switching between profiles, controlling lighting, etc).

i2c pull-ups live in the MCU module, as does the neopixel series resistor.
### Key module

Each key module holds a 74HC165 shift register, to be able to transmit all of its up to seven keys in a serial stream on pin 5, controlled by pin 4 and 6. Each module also has neopixel RGB LEDs, one under each key, and key switch sockets so the user can use any switch they want.

Modules connect via three pin headers (male, M20-791R) on the right side and matching sockets on the left side. The connectors are laid flat on the PCB and jut out past the board edge so modules click straight into each other. The enclosure chassis reinforces the joint mechanically so stress isn't borne by the solder joints.

Each module has decoupling capacitors for both the 165 and NeoPixel data lines.
### Terminator module
The leftmost column module has minimal electronics to provide a passive termination that causes the MCU's key-scan chain to report end-of-chain.

This is performed by simply tying pin 5 (DATA) high, so that when the last column's 165 pulls serial input from it, it gets all-ones, which will subsequently set the highest bit as 1, which the MCU can then interpret as "this is the dummy terminal module and we can ignore the whole byte and also stop reading".

### End-of-chain detection protocol
Each key module has the 74HC165's H input (pin 6) tied to GND, so bit 7 of every real column's byte is always 0. The terminator feeds all-ones (0xFF) into the chain. The MCU therefore scans by clocking out bytes one at a time, using a simple rule:

- **Byte has bit 7 set (0xFF)** → this is the sentinel from the terminator. Stop reading, chain is done.
- **Byte has bit 7 clear** → this is a real column. Bits 0–6 encode key states.

This makes the number of columns dynamic — no hardcoded column count, no configuration. The MCU just keeps reading until it hits the sentinel.

### Bit ordering
After parallel load (PL pulse), the 74HC165 shifts bits out of QH in this order: **H first, then G, F, E, D, C, B, A last**. This matters for firmware: the first bit the MCU reads from a real column corresponds to input H (which in our design is always 0), and the last bit corresponds to input A (key #1 / the bottom key).

## Bill Of Materials

| Identifier     | Count                    | Description                                                |
| -------------- | ------------------------ | ---------------------------------------------------------- |
| nice!nano v2.9 | 1                        |                                                            |
| 74HC165        | 1 x key module count     | Shift register for reporting key states over a serial line |
| 100 nF 0805    | 1 x key module count     | Decoupling for the 165, on the front next to it             |
| hanxia HX PM2.54-1x3P WT | 3 x module count | 1×3 horizontal SMT socket (female), left board edge; Harwin M20-7910342R fits the same pads |
| hanxia HX PZ2.54-1x3P WT | 3 x module count | 1×3 horizontal SMT pin header (male), right board edge; Harwin M20-8890345R fits the same pads |

### Board geometry at the joint

Numbers the key module PCB and the case both build on (the case still has to be redrawn for them):

| Item | Value | Why |
| ---- | ----- | --- |
| Board width | 16.85 mm | choc hotswap footprint needs 6.95 mm left and 9.9 mm right of the key center (the +3.3V socket pad plus 0.3 mm edge clearance); keys stay centered on the case, the board is just narrower |
| Header origin (pad row) | 4.1 mm inside the right edge | body face flush with the board edge, all 6 mm of pin overhang |
| Socket origin (pad row) | 9.15 mm inside the left edge | mouth overhangs the board by 2.0 mm, flush with a 1.8 mm wall plus 0.2 mm clearance |
| Board-to-board gap | 4.0 mm | two 1.8 mm walls plus clearance; gives 4.0 mm pin insertion, 1.5 mm past the socket contact point |
| Pin bend | 2.0 mm from the header body, 8° | at the joint plane; the jig in case/roamy-v3.py bends this |
| Connector rows | y = 58.1, 77.1 and 115.1 mm in the PCB file (keys at 51.93, 70.93, 89.86, 108.93, 128.0) | the gaps between keys 5/4, 4/3 and 2/1; the 3/2 gap holds the 165 and its pull-downs |

The hotswap socket sits on the back **above** each key (toward lower y), pads at x 74.225 (key net) and 85.775 (+3.3V), contact holes at (77.5, key - 5.95) and (82.5, key - 3.75); footprint `Library:Kailh_choc_v1_hotswap`, generated by `tools/choc_footprint.py` from the Kailh PG1350 drawing. Each connector row sits between the locating pins of the key above it and the socket of the key below it, with 0.2 mm or more to both courtyards.

KiCad footprints for both live in `KeyModule/Library.pretty` and are generated from the datasheet dimensions by `tools/harwin_footprints.py`; edit the script, not the `.kicad_mod` files.

## Schematic
![[KeyModuleSchematic.png]]

MCU module
* nice!nano connects to:
	* Battery
	* pin ? to interconnect pin 1 (+5V)
	* pin ? to interconnect pin 2 (+3.3V)
	* pin ? to interconnect pin 3 (GND)
	* pin ? to interconnect pin 4 (CLK)
	* pin ? to interconnect pin 5 (DATA)
	* pin ? to interconnect pin 6 (PL)
	* ... ignore the rest for now

Key module
* From MCU-facing interconnect (right side)
	* Pin 1 (+5V) through to terminal-facing (left side) interconnect  pin 1
	* Pin 2 (+3.3V)...
		* through to left interconnect pin 2, and
		* to the 165's pin 16 (VCC)
		* to the left leg of all the key switches, and
	* Pin 3 (GND)...
		* to the 165's pin 8 (GND), and
		* to all the pulldown resistors, and
		* through to left interconnect pin 3
	* Pin 4 (CLK) ...
		* through to left interconnect pin 4
		* to the 165's pin 2 (CLK)
	* Pin 5 (DATA) to the 165's pin 9 (QH, Serial Output)
	* Pin 6 (PL)...
		* through to left interconnect pin 6, and
		* to the 165's pin 1 (SH/LD aka PL)
* From the 165
	* Pin 11 (A) to bottom-most key switch's right leg, AND through a 10K resistor to ground
	* Pin 12 (B) to key #2 AND pull-down resistor
	* Pin 13 (C) to key #3 AND pull-down resistor
	* Pin 14 (D) to key #4 AND pull-down resistor
	* Pin 3 (E) to key #5 AND pull-down resistor, or GND if key unavailable
	* Pin 4 (F) to key #6 AND pull-down resistor, or GND if key unavailable
	* Pin 5 (G) to key #7 AND pull-down resistor, or GND if key unavailable
	* Pin 6 (H) to GND
	* Pin 15 (CLK INH) to GND
	* Pin 10 (SER, Serial Input) to left interconnect pin 5

Terminator module
* From incoming interconnect
	* Pin 2 (+3.3V) to the same interconnect pin 5 (DATA)