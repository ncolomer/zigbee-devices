#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

enum class LedCommandType {
  OFF,
  ON,
  BLINK,
  STOP
};

struct LedCommand {
  LedCommandType type;
  struct {
    uint32_t period;  // milliseconds
    float duty;       // 0.0 to 1.0
    int8_t count;    // -1 for infinite, 0-127 for finite count
  } parameters;
};

class StatusLed {
public:
  StatusLed(uint8_t pin);
  ~StatusLed();

  void off();
  void on();
  void blink(uint32_t period, float duty = 0.5, int8_t count = -1);
  void stopAndWait(uint32_t timeoutMs = 5000);

private:
  uint8_t _pin;
  QueueHandle_t _commandQueue;
  TaskHandle_t _taskHandle;
  SemaphoreHandle_t _stopSemaphore;

  static void taskFunction(void* parameter);
  void runTask();
};

#endif
