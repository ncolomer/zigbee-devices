#ifndef METERS_H
#define METERS_H

#include <Arduino.h>

#define METERS 2

// Reed switch pulse inputs, normally open to GND. One PulseEvent is pushed per closure.
class Meters {
public:
  void begin(QueueHandle_t events);

private:
  struct Line {
    uint8_t pin;
    TaskHandle_t task;
    volatile uint32_t last_us;  // last edge, set in ISR
    volatile uint32_t edges;    // edge count, debug only
    bool closed;                // pulse counted, waiting for re-arm; task only
  };

  QueueHandle_t _events;
  Line _lines[METERS];

  static void IRAM_ATTR onEdge(void *arg);
  static void taskFunction(void *arg);
  void runTask();
};

#endif
