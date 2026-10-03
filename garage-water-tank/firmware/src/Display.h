#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// All drawing is serialized by a mutex, so the pairing task and the app can share the panel.
class Display {
public:
  Display();
  bool begin();
  void setOn(bool on);
  void showUnknown();
  // markPct: high-water mark, NaN for none
  void showLevel(float pct, float markPct);

  // Blinks an empty screen / wifi icon (1 s each, empty first) until stopPairing().
  // Level and "?" draws are ignored meanwhile, so the caller redraws after stopPairing().
  void startPairing();
  void stopPairing();
  bool isPairing() { return _pairing; }

private:
  Adafruit_SSD1306 _oled;
  SemaphoreHandle_t _mutex;
  TaskHandle_t _task;
  volatile bool _pairing;
  bool _present;

  static void taskFunction(void *parameter);
  void runTask();
};

#endif
