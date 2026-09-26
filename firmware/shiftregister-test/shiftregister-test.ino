#include <M5Unified.h>

/*
  74HC165 chain reader + visualization on M5StickC Plus (M5Unified)

  Wiring (first 165 in chain):
    GPIO26 -> QH (serial out)
    GPIO25 -> CLK
    GPIO0  -> SH/LD (active low)

  Chaining (chip0 is the one wired to the M5Stick):
    ... -> QH of chip2 -> SER of chip1, QH of chip1 -> SER of chip0
    Read order is chip0 first, then chip1, ...; within a chip H first, A last.

  Configure how many bits/chips you want here:
*/
constexpr int PIN_QH   = 26; // DATA_OUT // green
constexpr int PIN_CLK  = 25; // CLK      // yellow
constexpr int PIN_SHLD = 0; // PL        // orange

constexpr int BITS_PER_CHIP = 8;
constexpr int NUM_CHIPS     = 1;                 // <-- set to 2, 3, ...
constexpr int N_BITS        = NUM_CHIPS * BITS_PER_CHIP;

static_assert(N_BITS > 0, "N_BITS must be > 0");
static_assert(N_BITS <= 128, "Increase storage if you want >128 bits");

// --- Display ---
static M5Canvas canvas; // off-screen buffer

// --- Bit storage (up to 128 bits) ---
struct BitBuffer {
  static constexpr int kMaxBits = 128;
  uint8_t bytes[(kMaxBits + 7) / 8] = {0};
  int bitCount = 0;

  void clear(int nBits) {
    bitCount = nBits;
    const int nBytes = (nBits + 7) / 8;
    for (int i = 0; i < nBytes; i++) bytes[i] = 0;
  }

  // index: 0..bitCount-1, where index 0 is the FIRST bit shifted out of QH
  bool get(int index) const {
    const int byteIndex = index / 8;
    const int bitIndex  = index % 8;
    return (bytes[byteIndex] >> bitIndex) & 0x01;
  }

  void set(int index, bool v) {
    const int byteIndex = index / 8;
    const int bitIndex  = index % 8;
    if (v) bytes[byteIndex] |=  (1u << bitIndex);
    else   bytes[byteIndex] &= ~(1u << bitIndex);
  }
};

// --- 165 reader ---
struct HC165Chain {
  int pinQH, pinCLK, pinSHLD;
  explicit HC165Chain(int qh, int clk, int shld) : pinQH(qh), pinCLK(clk), pinSHLD(shld) {}

  void begin() const {
    pinMode(pinQH, INPUT);
    pinMode(pinCLK, OUTPUT);
    digitalWrite(pinCLK, LOW);
    pinMode(pinSHLD, OUTPUT);
    digitalWrite(pinSHLD, HIGH); // idle high = shift mode
  }

  static inline void pulseClock(int pinCLK) {
    digitalWrite(pinCLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(pinCLK, LOW);
    delayMicroseconds(1);
  }

  void latch() const {
    digitalWrite(pinSHLD, LOW);
    delayMicroseconds(3);
    digitalWrite(pinSHLD, HIGH);
    delayMicroseconds(3);
  }

  // Reads nBits bits into out. Bit 0 is the first bit observed on QH after latch.
  void readBits(BitBuffer& out, int nBits) const {
    out.clear(nBits);
    latch();
    for (int i = 0; i < nBits; i++) {
      const bool bit = digitalRead(pinQH);
      out.set(i, bit);
      pulseClock(pinCLK);
    }
  }
};

// --- Visualization ---
static const uint16_t kPalette[] = {
  TFT_PURPLE, TFT_ORANGE, TFT_MAGENTA, TFT_GREEN,
  TFT_CYAN,   TFT_BLUE,   TFT_RED,     TFT_YELLOW,
  TFT_PINK,   TFT_NAVY,   TFT_DARKGREEN, TFT_MAROON,
  TFT_OLIVE,  TFT_SKYBLUE, TFT_BROWN,  TFT_LIGHTGREY
};
constexpr int kPaletteN = sizeof(kPalette) / sizeof(kPalette[0]);

void drawBitsToCanvas(const BitBuffer& bits)
{
  const int w = canvas.width();
  const int h = canvas.height();

  // Choose grid automatically: near-square
  int cols = (int)ceil(sqrt((double)bits.bitCount));
  if (cols < 1) cols = 1;
  int rows = (bits.bitCount + cols - 1) / cols;

  const int pad = 5;

  const int cellW = (w - pad * (cols + 1)) / cols;
  const int cellH = (h - pad * (rows + 1)) / rows;

  // Keep cells square-ish
  int size = (cellW < cellH) ? cellW : cellH;
  if (size < 4) size = 4; // avoid degeneracy

  canvas.fillScreen(TFT_BLACK);

  for (int i = 0; i < bits.bitCount; i++)
  {
    const int r = i / cols;
    const int c = i % cols;

    const int x = pad + c * (size + pad);
    const int y = pad + r * (size + pad);

    // If we overflow screen due to rounding, stop drawing.
    if (x + size > w || y + size > h) break;

    const bool pressed = bits.get(i);
    const uint16_t col = kPalette[i % kPaletteN];

    if (pressed) {
      canvas.fillRect(x, y, size, size, col);
      canvas.drawRect(x, y, size, size, TFT_WHITE);
    } else {
      canvas.drawRect(x, y, size, size, col);
    }
  }

  // Print bits (LSB-first as stored: bit0 is first shifted out)
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextSize(1);
  canvas.setCursor(2, h - 10);
  canvas.printf("%d bits: ", bits.bitCount);
  for (int i = 0; i < bits.bitCount; i++) canvas.print(bits.get(i) ? '1' : '0');
}

// --- Global instances ---
static HC165Chain chain(PIN_QH, PIN_CLK, PIN_SHLD);
static BitBuffer  bits;

void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);

  chain.begin();

  M5.Display.setRotation(1);

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.setTextWrap(false);
}

void loop()
{
  M5.update();

  chain.readBits(bits, N_BITS);

  // Display
  drawBitsToCanvas(bits);
  canvas.pushSprite(&M5.Display, 0, 0);

  // Serial (same order as displayed: bit0 first)
  Serial.printf("%d bits: ", N_BITS);
  for (int i = 0; i < N_BITS; i++) Serial.print(bits.get(i) ? '1' : '0');
  Serial.println();

  delay(100);
}