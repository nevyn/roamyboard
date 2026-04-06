> 💡 This document describes the electrical wiring of the [[Modular Design (v3)]]. 

## Concepts
### Column modules and interconnects

Each column is its own module. There are three kinds of modules: MCU, Key and Terminator. The MCU is the rightmost, terminator the leftmost, and an arbitrary number of key modules in between.

The interconnect between each module has pins for:
- 5 VCC, 3.3 VCC and GND pins
- Key input: /PL, CLK and DATA for the shift register
- NeoPixel data line, to control RGB LEDs
- Future proofing: three pins for I2C (SDA, SCL and INT). Useful if we want touch pad, etc

v3.0 pinout is suggested to be this reduced set of pins, where 1 is the topmost pin
```
1     2     3     4     5     6      7     8    9
+5V   +3.3V GND   CLK   DATA  /PL    LED   SDA  SCL   
```
### MCU module
The MCU module contains a nice!nano 2.0, battery, USB-C for wired connection and charging, and power switch. Possibly also an OLED display, and maybe a rotary encoder for settings (pairing, switching between profiles, controlling lighting, etc).

i2c pull-ups live in the MCU module, as does the neopixel series resistor.
### Key module

Each key module holds a 74HC165 shift register, to be able to transmit all of its up to seven keys in a serial stream on pin 5, controlled by pin 4 and 6. Each module also has neopixel RGB LEDs, one under each key, and key switch sockets so the user can use any switch they want.

Modules connect via pogo pins on the right side and pads on the left side that accept the next module. The connector mechanic in the enclosure ensures alignment of the pogo pins.

Each module has decoupling capacitors for both the 165 and NeoPixel data lines.
### Terminator module
The leftmost column module has minimal electronics to provide a passive termination that causes the MCU's key-scan chain to report end-of-chain.

## Bill Of Materials

| Identifier     | Count                | Description                                                |
| -------------- | -------------------- | ---------------------------------------------------------- |
| nice!nano v2.9 | 1                    |                                                            |
| 74HC165        | 1 x key module count | Shift register for reporting key states over a serial line |
| ??             | 1 x module count - 1 | Pogo pin for module interconnect (male)                    |
| ??             | 1 x module count - 1 | Pogo pin for module interconnect (female)                  |

## Schematic