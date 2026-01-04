![[Modular sketch.png]]
- Each column is its own module, so that the user can choose how wide to make their keyboard by connecting multiple together.
	- Each column module has 6 keys, creating a full-height keyboard with F keys. (We could also make 4 or 5 key height columns and I think everything should still work)
	- The rightmost column module is the brains, with:
		- microcontroller, battery, USB-C for wired connection and charging, and power switch.
		- Maybe also OLED screen.
		- Maybe a rotary encoder for settings? Pairing, switching between profiles, controlling lighting, etc.
		- It also has the rightmost fastener for the harness
	- The leftmost column "module" has minimal electronics to provides a passive termination that causes the MCU's key-scan chain to report end-of-chain. 
		- It also has the leftmost fastener for the harness
	- Modules are not intended to be connected/disconnected while powered.
- Each column module has its own PCB
	- With pogo pins to connect to the previous column on the right
	- And pads to accept additional modules on the left
	- And of course key switch sockets, so user can use any switch they want
	- And of course, neopixel RGB LEDs, one under each key
	- A 74HC165 shift register to store key states, and transmit its own and each previous column's key data over a serial bus to the MCU
- Each column module mechanically joins together by sliding into grooves on the previous module's left side, where the pads are. Similar to Switch Joycons.
	- This connector mechanic in the enclosure ensures alignment of the pogo pins
- The connector between each module has pins for:
	- 2x 5 VCC, 1x 3.3 VCC and 3x GND pins
	- Key input: /PL, CLK and DATA for the shift register
	- Future proofing: three pins for I2C (SDA, SCL and INT). Useful if we want touch pad, etc
	- NeoPixel data line, to control RGB LEDs
- Electrical implementation details:
	- i2c pull-ups live in the MCU module, as does the neopixel series resistor
	- Each module has decoupling capacitors for both the 165 and neopixel data lines

## Connector pinout
### alt 1
two 5V to make sure to have juice for LEDs
GND near each important signal line
```
1    2     3     4    5    6    7     8     9    10   11   12    13    14
GND  +5V   +5V   GND  LED  GND  CLK   DATA  /PL  GND  SDA  SCL   INT   +3.3V
```
### alt 2
pack everything important into just 9 pins so we can go single column. Skip power and ground redundancy, and skip the i2c interrupt line.
```
1    2     3     4     5    6    7     8     9
GND  +3.3V CLK   DATA  /PL  +5V  LED   SDA  SCL   
```



## Component breakdown

- Pogo pin connector: TBD
- Shift register: 74HC165
