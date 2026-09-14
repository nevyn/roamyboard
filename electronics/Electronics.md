> 💡 This document describes the electrical wiring of the [[Modular Design (v3)]]. 

> ✅ **Design status:** Validated on a hand-wired single-column prototype. The shift-register chain, the terminator sentinel trick, and the M5StickC-based test firmware are all confirmed working.
![[prototype column.png]]
## Concepts
### Column modules and interconnects

Each column is its own module. There are three kinds of modules: MCU, Key and Terminator. The MCU is the rightmost, terminator the leftmost, and an arbitrary number of key modules in between.

The interconnect between each module has 9 signals total:
- Power: +5V, +3.3V, GND
- Key input: /PL, CLK and DATA for the shift register
- NeoPixel data line, to control RGB LEDs
- Future proofing: three pins for I2C (SDA, SCL and INT). Useful if we want touch pad, etc

The 9 signals are distributed across **three 1×3 connector pairs** — top, middle, and bottom of the board. This approach was chosen over a single 9-pin connector because:
- Distributing the connection across three points along the board edge provides better mechanical stability between modules than a single point would
- Matching 9-pin pogo-pin connectors were prohibitively expensive (~$10 per pin header, 12+ per keyboard)
- Three 1×3 connectors fit within the keyswitch courtyard constraints on the PCB, whereas a single 1×9 did not

Connectors used: **Harwin M20-791R series** pin header (male) and matching socket. See [datasheet](https://content.harwin.com/m/0e0398fdb977d498/original/DRG-02613-Technical-Drawing-Datasheet-M20-791R-pdf.pdf). The connectors are laid flat and protrude past the board edge so modules can click into each other straight-on.

v3.0 pinout (1 = topmost pin within each connector):
```
Top connector    Middle connector    Bottom connector
1  2  3          1  2  3             1  2  3
?  ?  ?          ?  ?  ?             ?  ?  ?
```
_Pin assignments per connector: TBD — to be documented based on final PCB routing._
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
| Harwin M20-7910342R   | 3 x module count  | 1×3 horizontal SMT socket (female), mounts at board edge   |
| Harwin M20-8890345*   | 3 x module count  | 1×3 horizontal SMT pin header (male), mating partner       |

_*Male part number to be verified against Harwin's ordering code before purchase — the horizontal SMT male series uses an ambiguous suffix pattern in the catalog._

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