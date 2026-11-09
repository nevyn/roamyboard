// Drives and probes one roamyboard module's 74HC165 and interconnect lines from an
// Adafruit HUZZAH32 (ESP32 Feather, 3.3 V I/O). Serial at 115200; send "help".
//
// Wiring (module header pins plug into the MCU module; pin 1 is at the SW5 end):
//   GPIO 26     -> header J1 pin 2 (+3.3V)      Feather GND -> header J1 pin 3 (GND)
//   GPIO 27     -> header J2 pin 1 (CLK)        GPIO 33     -> header J2 pin 2 (/PL)
//   GPIO 32     -> header J2 pin 3 (U1 QH)      GPIO 14     -> middle socket pin 3 (U1 SER)
//   GPIO 25     -> header J3 pin 1 (INT)        GPIO 23     -> header J3 pin 2 (SDA)
//   GPIO 22     -> header J3 pin 3 (SCL)        GPIO 34     -> top socket pin 2 (+3.3V sense)
//   GPIO 4      -> SW4 pad 2 (D3, U1 pin 14; R4 pulls it down)

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>

constexpr int PIN_CLK = 27;
constexpr int PIN_PL = 33;
constexpr int PIN_QH = 32;
constexpr int PIN_SER = 14;
constexpr int PIN_INT = 25;
constexpr int PIN_SDA = 23;
constexpr int PIN_SCL = 22;
constexpr int PIN_VCC = 34;  // input only, no pulls
constexpr int PIN_SW4 = 4;   // driven high to press, released to let go; never driven low
constexpr int PIN_PWR = 26;  // supplies the module, so the probe can power-cycle it

constexpr uint8_t TRACKPAD_ADDR = 0x74;
constexpr uint32_t SCAN_PERIOD_MS = 20;
constexpr uint32_t HEARTBEAT_MS = 2000;
constexpr int DEAD_SCANS = 3;
constexpr int VCC_LOST_MV = 2800;
constexpr int VCC_BACK_MV = 3000;

struct Line {
  const char *name;
  int pin;
  bool driven;  // the probe drives it in normal operation
};

const Line kLines[] = {
    {"CLK", PIN_CLK, true}, {"/PL", PIN_PL, true},  {"SER", PIN_SER, true}, {"QH", PIN_QH, false},
    {"INT", PIN_INT, false}, {"SDA", PIN_SDA, false}, {"SCL", PIN_SCL, false}, {"SW4", PIN_SW4, false},
};

bool clkIdleHigh = false;  // false: like the M5 tester; true: like the keyboard (SPI mode 2)
char serMode = '1';        // '0', '1' or 'z' (released)
bool i2cPullups = false;
bool watching = true;
int nBits = 16;
uint32_t pulseUs = 2;      // clock half period and /PL width

bool powered = false;
bool vccPresent = true;
uint32_t vccGoodSince = 0;
uint8_t lastBytes[4] = {0xA5, 0xA5, 0xA5, 0xA5};  // never a real first read
int deadStreak = 0;
bool reportedDead = false;
uint32_t lastScan = 0;
uint32_t lastHeartbeat = 0;
uint32_t scanCount = 0;
int lastSw4 = -1;
bool sw4Pressed = false;

void printTime() { Serial.printf("t=%lu ", (unsigned long)millis()); }

void applyI2cPins() {
  pinMode(PIN_SDA, i2cPullups ? INPUT_PULLUP : INPUT);
  pinMode(PIN_SCL, i2cPullups ? INPUT_PULLUP : INPUT);
}

void applySer() {
  if (serMode == 'z') {
    pinMode(PIN_SER, INPUT);
  } else {
    pinMode(PIN_SER, OUTPUT);
    digitalWrite(PIN_SER, serMode == '1');
  }
}

void applyIdle() {
  pinMode(PIN_CLK, OUTPUT);
  digitalWrite(PIN_CLK, clkIdleHigh);
  pinMode(PIN_PL, OUTPUT);
  digitalWrite(PIN_PL, HIGH);
  pinMode(PIN_QH, INPUT);
  pinMode(PIN_INT, INPUT);
  pinMode(PIN_SW4, sw4Pressed ? OUTPUT : INPUT);
  if (sw4Pressed) digitalWrite(PIN_SW4, HIGH);
  applySer();
  applyI2cPins();
}

// Every module line low, so that nothing back-powers the module through U1's inputs.
void quiesce() {
  for (int pin : {PIN_CLK, PIN_PL, PIN_SER}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void powerOff() {
  quiesce();
  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_PWR, LOW);
  powered = false;
}

void powerOn() {
  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_PWR, HIGH);
  delay(50);
  applyIdle();
  powered = true;
  vccPresent = true;
  memset(lastBytes, 0xA5, sizeof(lastBytes));
  deadStreak = 0;
  reportedDead = false;
  scanCount = 0;
}

int vccMillivolts() { return analogReadMilliVolts(PIN_VCC); }

// Loads U1 and shifts n bits out of QH into bytes, MSB first, so byte 0 reads like the
// firmware's chain dump (bit 7 is input H).
void scan(uint8_t *bytes, int n) {
  memset(bytes, 0, (n + 7) / 8);
  digitalWrite(PIN_PL, LOW);
  delayMicroseconds(pulseUs);
  digitalWrite(PIN_PL, HIGH);
  delayMicroseconds(pulseUs);
  for (int i = 0; i < n; i++) {
    if (clkIdleHigh) {
      digitalWrite(PIN_CLK, LOW);
      delayMicroseconds(pulseUs);
    }
    if (digitalRead(PIN_QH)) {
      bytes[i / 8] |= 0x80 >> (i % 8);
    }
    digitalWrite(PIN_CLK, HIGH);
    delayMicroseconds(pulseUs);
    if (!clkIdleHigh) {
      digitalWrite(PIN_CLK, LOW);
      delayMicroseconds(pulseUs);
    }
  }
}

void printScan(const uint8_t *bytes, int n) {
  for (int i = 0; i < (n + 7) / 8; i++) {
    Serial.printf("%02X ", bytes[i]);
  }
  const uint8_t b = bytes[0];
  Serial.printf("| H%d G%d F%d E%d D%d C%d B%d A%d", (b >> 7) & 1, (b >> 6) & 1, (b >> 5) & 1,
                (b >> 4) & 1, (b >> 3) & 1, (b >> 2) & 1, (b >> 1) & 1, b & 1);
  if (b == 0x00) {
    Serial.print(" | DEAD-LOOKING (no G marker)");
  } else if (b == 0xFF) {
    Serial.print(" | ALL ONES");
  } else if ((b & 0xC0) == 0x40) {
    Serial.print(" | ok");
  }
  Serial.println();
}

const char *classify(int up, int down) {
  if (up && !down) return "free (nothing on the module drives it)";
  if (up && down) return "HELD HIGH by the module";
  if (!up && !down) return "HELD LOW by the module";
  return "inverted?";
}

// For each line: the level while the probe drives it (read back from the pad), then the
// level with the probe released, and whether the module holds it against each pull.
void pinReport() {
  printTime();
  Serial.printf("pins: vcc=%dmV clkIdle=%s ser=%c i2cPullups=%d\n", vccMillivolts(),
                clkIdleHigh ? "high" : "low", serMode, i2cPullups);
  for (const Line &l : kLines) {
    Serial.printf("  %-4s", l.name);
    if (l.driven) {
      pinMode(l.pin, OUTPUT);
      digitalWrite(l.pin, HIGH);
      delayMicroseconds(50);
      const int readHigh = digitalRead(l.pin);
      digitalWrite(l.pin, LOW);
      delayMicroseconds(50);
      const int readLow = digitalRead(l.pin);
      Serial.printf(" drive1->%d drive0->%d", readHigh, readLow);
    }
    pinMode(l.pin, INPUT);
    delay(2);
    if (digitalPinToAnalogChannel(l.pin) >= 0) {
      Serial.printf(" released=%4dmV", analogReadMilliVolts(l.pin));
    } else {
      Serial.printf(" released=%d     ", digitalRead(l.pin));
    }
    pinMode(l.pin, INPUT_PULLUP);
    delay(2);
    const int up = digitalRead(l.pin);
    pinMode(l.pin, INPUT_PULLDOWN);
    delay(2);
    const int down = digitalRead(l.pin);
    Serial.printf(" pullup->%d pulldown->%d: %s\n", up, down, classify(up, down));
  }
  applyIdle();
}

void i2cProbe() {
  printTime();
  Serial.println("i2c: probing the trackpad at 0x74 (internal pull-ups, 100 kHz)");
  Wire.begin(PIN_SDA, PIN_SCL, 100000);
  Wire.setTimeOut(200);
  Wire.beginTransmission(TRACKPAD_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);  // product number, 16-bit register address
  const uint8_t err = Wire.endTransmission(false);
  if (err) {
    Serial.printf("  address write failed: %u (2 = NACK on address)\n", err);
  } else {
    const int got = Wire.requestFrom(TRACKPAD_ADDR, (uint8_t)2);
    if (got == 2) {
      const int hi = Wire.read();
      const int lo = Wire.read();
      Serial.printf("  product number %d (IQS572 reads 58)\n", (hi << 8) | lo);
    } else {
      Serial.printf("  read returned %d bytes\n", got);
    }
  }
  Wire.beginTransmission(TRACKPAD_ADDR);
  Wire.write(0xEE);
  Wire.write(0xEE);
  Wire.write(0x00);  // end the communication window
  Serial.printf("  end window: %u\n", Wire.endTransmission());
  Wire.end();
  applyI2cPins();
}

void help() {
  Serial.println(
      "commands:\n"
      "  r            one scan\n"
      "  w / s        watch on / off (scan every 20 ms, print changes, report pins when dead)\n"
      "  p            pin report: who drives each line\n"
      "  clk low|high clock idle level (low: M5 tester, high: keyboard)\n"
      "  ser 0|1|z    drive SER low, high, or release it\n"
      "  bits N       bits per scan (8..32)\n"
      "  us N         clock half period and /PL width in microseconds\n"
      "  i2c          probe the trackpad over I2C\n"
      "  pull 0|1     internal pull-ups on SDA and SCL\n"
      "  off / on     module power off (all lines low) / on\n"
      "  cycle N      power off for N ms, then on\n"
      "  press / release   drive SW4 high / release it\n"
      "  tap N        press SW4 for N ms\n"
      "  set PIN 0|1|z  raw GPIO control\n"
      "  v            module supply voltage\n"
      "  q            quiesce: CLK, /PL, SER low (before pulling module power)\n"
      "  idle         restore idle levels");
}

void handle(String cmd) {
  cmd.trim();
  if (cmd.isEmpty()) return;
  printTime();
  Serial.printf("> %s\n", cmd.c_str());
  uint8_t bytes[4];
  if (cmd == "help" || cmd == "?") {
    help();
  } else if (cmd == "r") {
    scan(bytes, nBits);
    printTime();
    printScan(bytes, nBits);
  } else if (cmd == "w") {
    watching = true;
    memset(lastBytes, 0xA5, sizeof(lastBytes));
  } else if (cmd == "s") {
    watching = false;
  } else if (cmd == "p") {
    pinReport();
  } else if (cmd == "clk low" || cmd == "clk high") {
    clkIdleHigh = cmd == "clk high";
    applyIdle();
  } else if (cmd.startsWith("ser ") && cmd.length() == 5 && strchr("01z", cmd[4])) {
    serMode = cmd[4];
    applySer();
  } else if (cmd.startsWith("bits ")) {
    nBits = constrain(cmd.substring(5).toInt(), 8, 32);
  } else if (cmd.startsWith("us ")) {
    pulseUs = constrain(cmd.substring(3).toInt(), 1, 10000);
  } else if (cmd == "i2c") {
    i2cProbe();
  } else if (cmd == "pull 0" || cmd == "pull 1") {
    i2cPullups = cmd == "pull 1";
    applyI2cPins();
  } else if (cmd == "off") {
    powerOff();
    printTime();
    Serial.println("module power off");
  } else if (cmd == "on") {
    powerOn();
    printTime();
    Serial.printf("module power on, vcc=%dmV\n", vccMillivolts());
  } else if (cmd.startsWith("cycle")) {
    const int ms = cmd.length() > 6 ? cmd.substring(6).toInt() : 500;
    powerOff();
    delay(constrain(ms, 1, 30000));
    powerOn();
    printTime();
    Serial.printf("module power cycled (%d ms off), vcc=%dmV\n", ms, vccMillivolts());
  } else if (cmd == "press" || cmd == "release") {
    sw4Pressed = cmd == "press";
    applyIdle();
  } else if (cmd.startsWith("tap")) {
    sw4Pressed = true;
    applyIdle();
    delay(constrain(cmd.length() > 4 ? cmd.substring(4).toInt() : 100, 1, 10000));
    sw4Pressed = false;
    applyIdle();
  } else if (cmd.startsWith("set ")) {
    const int sp = cmd.indexOf(' ', 4);
    const int pin = cmd.substring(4, sp).toInt();
    const char v = cmd[sp + 1];
    if (v == 'z') {
      pinMode(pin, INPUT);
    } else {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, v == '1');
    }
    printTime();
    Serial.printf("GPIO %d -> %c\n", pin, v);
  } else if (cmd == "v") {
    delay(20);
    printTime();
    Serial.printf("vcc=%dmV qh=%d\n", vccMillivolts(), digitalRead(PIN_QH));
  } else if (cmd == "q") {
    watching = false;
    quiesce();
  } else if (cmd == "idle") {
    applyIdle();
  } else {
    Serial.println("unknown command; send help");
  }
}

void setup() {
  Serial.begin(115200);
  analogSetPinAttenuation(PIN_VCC, ADC_11db);
  powerOff();
  gpio_set_drive_capability((gpio_num_t)PIN_PWR, GPIO_DRIVE_CAP_3);
  delay(200);
  powerOn();
  delay(150);
  Serial.println();
  printTime();
  Serial.println("chain_probe ready; send help");
  pinReport();
}

void loop() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      handle(line);
      line = "";
    } else {
      line += c;
    }
  }

  const uint32_t now = millis();
  if (!powered || !watching || now - lastScan < SCAN_PERIOD_MS) return;
  lastScan = now;

  const int vcc = vccMillivolts();
  if (vccPresent && vcc < VCC_LOST_MV) {
    vccPresent = false;
    quiesce();
    printTime();
    Serial.printf("VCC LOST: %dmV; CLK, /PL, SER held low\n", vcc);
    return;
  }
  if (!vccPresent) {
    if (vcc < VCC_BACK_MV) {
      vccGoodSince = 0;
      return;
    }
    if (!vccGoodSince) vccGoodSince = now;
    if (now - vccGoodSince < 200) return;
    vccPresent = true;
    vccGoodSince = 0;
    applyIdle();
    printTime();
    Serial.printf("VCC BACK: %dmV\n", vcc);
    memset(lastBytes, 0xA5, sizeof(lastBytes));
  }

  const int sw4 = digitalRead(PIN_SW4);
  if (sw4 != lastSw4) {
    lastSw4 = sw4;
    printTime();
    Serial.printf("SW4 pad %s\n", sw4 ? "HIGH (pressed)" : "low");
  }

  uint8_t bytes[4];
  scan(bytes, nBits);
  scanCount++;
  const int nBytes = (nBits + 7) / 8;
  if (memcmp(bytes, lastBytes, nBytes)) {
    memcpy(lastBytes, bytes, nBytes);
    printTime();
    printScan(bytes, nBits);
  }

  if (bytes[0] == 0x00) {
    if (++deadStreak >= DEAD_SCANS && !reportedDead) {
      reportedDead = true;
      printTime();
      Serial.printf("DEAD after %lu scans; pin report follows\n", (unsigned long)scanCount);
      pinReport();
    }
  } else {
    deadStreak = 0;
    reportedDead = false;
  }

  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    printTime();
    Serial.printf("hb vcc=%dmV int=%d sda=%d scl=%d scans=%lu\n", vcc, digitalRead(PIN_INT),
                  digitalRead(PIN_SDA), digitalRead(PIN_SCL), (unsigned long)scanCount);
  }
}
