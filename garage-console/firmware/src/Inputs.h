#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>

#define CHANNELS 16

// 16 switches on a PCF8575; closed contact = pin low. Changes are debounced and pushed
// to the queue as InputEvent.
class Inputs {
public:
  void begin(QueueHandle_t events);
  // Bit n set = switch n closed
  uint16_t state() { return _state; }

private:
  QueueHandle_t _events;
  TaskHandle_t _task;
  volatile uint16_t _state = 0;

  uint16_t read();
  static void IRAM_ATTR onInterrupt(void *arg);
  static void taskFunction(void *arg);
  void runTask();
};

#endif
