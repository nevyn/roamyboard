# shiftregister-test

Arduino sketch for an M5StickC Plus (M5Unified) that clocks a chain of 74HC165s and shows every bit on the screen and on serial. Used for the first hardware test of the key module on 2026-09-25. The Arduino IDE copy lives in ~/Dev/Arduino/shiftregister-test; this is the same file.

Wiring to one key module, on its right-edge headers (J_RIGHT*, pin 1 is the topmost pin):

| M5StickC Plus | Key module | Signal |
| --- | --- | --- |
| 3V3 | J_RIGHT1 pin 2 | +3.3V |
| GND | J_RIGHT1 pin 3 | GND |
| GPIO25 | J_RIGHT2 pin 1 | CLK |
| GPIO0 | J_RIGHT2 pin 2 | /PL (SH/LD) |
| GPIO26 | J_RIGHT2 pin 3 | DATA_OUT (QH) |

Bit order on screen and serial, bit 0 first: the first bit out of QH is input H, so for one module bits 0..2 are the grounded inputs H, G, F (always 0), bit 3 is SW5 (input E), bit 4 SW4, bit 5 SW3, bit 6 SW2 and bit 7 SW1 (input A). With more modules set NUM_CHIPS; the module farthest from the M5Stick comes out first.

For a single module, J_LEFT2 pin 3 (DATA_IN) was wired to GND as a stand-in terminator, so the bits shifted in past the module read 0. The real terminator ties DATA high so the MCU sees 0xFF as the end of the chain (Electronics.md).
