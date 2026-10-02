#include "Display.h"
#include <Wire.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32
#define SCREEN_ADDRESS  0x3C

Display::Display() : _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {}

bool Display::begin() {
  Wire.begin();
  if (!_oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) return false;
  _oled.setRotation(1); // portrait: 32 wide x 128 tall
  _oled.dim(true);
  _oled.setTextSize(1);
  _oled.setTextColor(SSD1306_INVERSE);
  return true;
}

void Display::setOn(bool on) {
  _oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void Display::showUnknown() {
  _oled.clearDisplay();
  _oled.drawRect(0, 0, _oled.width(), _oled.height(), SSD1306_WHITE);
  _oled.setCursor(14, _oled.height() / 2);
  _oled.print("?");
  _oled.display();
}

void Display::showLevel(float pct, float markPct) {
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
  _oled.display();
}
