#include "Display.h"
#include <Wire.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32
#define SCREEN_ADDRESS  0x3C

#define PAIRING_PHASE_MS  1000
#define WIFI_ICON_SIZE    24

static const uint8_t WIFI_ICON[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0xFF, 0x80, 0x07, 0xFF, 0xE0, 0x1F, 0x81, 0xF8, 0x7C, 0x00, 0x3E,
  0xF0, 0x00, 0x0F, 0xC0, 0xFF, 0x03, 0x03, 0xFF, 0xC0, 0x0F, 0x81, 0xF0,
  0x1E, 0x00, 0x78, 0x08, 0x00, 0x10, 0x00, 0x7E, 0x00, 0x01, 0xFF, 0x80,
  0x01, 0xC3, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
  0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x18, 0x00
};

Display::Display()
  : _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1), _mutex(NULL), _task(NULL), _pairing(false) {}

bool Display::begin() {
  _mutex = xSemaphoreCreateMutex();
  Wire.begin();
  if (!_oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) return false;
  _oled.setRotation(1); // portrait: 32 wide x 128 tall
  _oled.dim(true);
  _oled.setTextSize(1);
  _oled.setTextColor(SSD1306_INVERSE);
  _oled.clearDisplay(); // the library starts with its splash logo in the buffer
  _oled.display();
  xTaskCreate(taskFunction, "pairing_screen", 3072, this, 1, &_task);
  return true;
}

void Display::setOn(bool on) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
  xSemaphoreGive(_mutex);
}

void Display::showUnknown() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_pairing) {
    xSemaphoreGive(_mutex);
    return;
  }
  _oled.clearDisplay();
  _oled.drawRect(0, 0, _oled.width(), _oled.height(), SSD1306_WHITE);
  _oled.setCursor(14, _oled.height() / 2);
  _oled.print("?");
  _oled.display();
  xSemaphoreGive(_mutex);
}

void Display::showLevel(float pct, float markPct) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_pairing) {
    xSemaphoreGive(_mutex);
    return;
  }
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
  xSemaphoreGive(_mutex);
}

void Display::startPairing() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _pairing = true;
  xSemaphoreGive(_mutex);
  xTaskNotifyGive(_task);
}

void Display::stopPairing() {
  // Taking the mutex guarantees the task is not mid-draw once we return
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _pairing = false;
  xSemaphoreGive(_mutex);
  xTaskNotifyGive(_task);
}

void Display::taskFunction(void *parameter) {
  static_cast<Display *>(parameter)->runTask();
}

void Display::runTask() {
  for (;;) {
    while (!_pairing) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Starts empty, so a quick join never shows the icon
    bool showIcon = false;
    while (_pairing) {
      xSemaphoreTake(_mutex, portMAX_DELAY);
      if (_pairing) {
        _oled.clearDisplay();
        if (showIcon) {
          _oled.drawBitmap((_oled.width() - WIFI_ICON_SIZE) / 2, (_oled.height() - WIFI_ICON_SIZE) / 2 - 2,
                           WIFI_ICON, WIFI_ICON_SIZE, WIFI_ICON_SIZE, SSD1306_WHITE);
        }
        _oled.display();
      }
      xSemaphoreGive(_mutex);

      // Returns early when stopPairing() notifies
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PAIRING_PHASE_MS));
      showIcon = !showIcon;
    }
  }
}
