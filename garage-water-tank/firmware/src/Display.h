#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Not thread-safe: only the main loop draws.
class Display {
public:
  Display();
  bool begin();
  void setOn(bool on);
  // pairingIcon: small wifi icon on top of the view
  void showUnknown(bool pairingIcon);
  // markPct: high-water mark, NaN for none
  void showLevel(float pct, float markPct, bool pairingIcon);

private:
  Adafruit_SSD1306 _oled;
  bool _present;

  void drawPairingIcon();
};

#endif
