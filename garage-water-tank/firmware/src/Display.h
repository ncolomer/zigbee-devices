#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class Display {
public:
  Display();
  bool begin();
  void setOn(bool on);
  void showUnknown();
  // markPct: high-water mark, NaN for none
  void showLevel(float pct, float markPct);

private:
  Adafruit_SSD1306 _oled;
};

#endif
