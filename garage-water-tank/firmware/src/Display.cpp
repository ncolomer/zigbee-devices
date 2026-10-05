#include "Display.h"
#include <Wire.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32
#define SCREEN_ADDRESS  0x3C

#define ICON_SIZE       16

static const uint8_t WIFI_ICON[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x3C, 0x3C, 0xE0, 0x07, 0xC3, 0xC3, 0x1F, 0xF8, 0x38, 0x1C,
  0x20, 0x04, 0x07, 0xE0, 0x0E, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00
};

Display::Display() : _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1), _present(false) {}

bool Display::begin() {
  // Adafruit's begin() doesn't check for an ACK, so probe first and run headless if absent
  Wire.begin();
  Wire.beginTransmission(SCREEN_ADDRESS);
  if (Wire.endTransmission() != 0) return false;
  if (!_oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) return false;
  _present = true;
  _oled.setRotation(1); // portrait: 32 wide x 128 tall
  _oled.dim(true);
  _oled.setTextSize(1);
  _oled.setTextColor(SSD1306_INVERSE);
  _oled.clearDisplay(); // the library starts with its splash logo in the buffer
  _oled.display();
  return true;
}

void Display::setOn(bool on) {
  if (!_present) return;
  _oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

// INVERSE keeps it readable over the level bar
void Display::drawPairingIcon() {
  _oled.drawBitmap((_oled.width() - ICON_SIZE) / 2, 3, WIFI_ICON, ICON_SIZE, ICON_SIZE, SSD1306_INVERSE);
}

void Display::showUnknown(bool pairingIcon) {
  if (!_present) return;
  _oled.clearDisplay();
  _oled.drawRect(0, 0, _oled.width(), _oled.height(), SSD1306_WHITE);
  _oled.setCursor(14, _oled.height() / 2);
  _oled.print("?");
  if (pairingIcon) drawPairingIcon();
  _oled.display();
}

void Display::showLevel(float pct, float markPct, bool pairingIcon) {
  if (!_present) return;
  const int16_t w = _oled.width();
  const int16_t h = _oled.height();
  const int16_t inner = h - 4;
  uint8_t level = constrain((int)roundf(pct), 0, 100);

  _oled.clearDisplay();
  _oled.drawRect(0, 0, w, h, SSD1306_WHITE);

  int16_t pixels = map(level, 0, 100, 0, inner);
  _oled.fillRect(2, 2 + inner - pixels, w - 4, pixels, SSD1306_WHITE);

  // Drawn only above the bar, so it vanishes once the level reaches the mark
  if (!isnan(markPct)) {
    int16_t markPixels = map(constrain((int)roundf(markPct), 0, 100), 0, 100, 0, inner);
    if (markPixels > pixels) _oled.drawFastHLine(2, 2 + inner - markPixels, w - 4, SSD1306_WHITE);
  }

  _oled.setCursor(level < 10 ? 11 : (level < 100 ? 8 : 4), h / 2);
  _oled.printf("%d%%", level);
  if (pairingIcon) drawPairingIcon();
  _oled.display();
}
