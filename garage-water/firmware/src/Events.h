#ifndef EVENTS_H
#define EVENTS_H

#include <Arduino.h>

// Everything that can wake the main loop; loop() is the only consumer.
struct PulseEvent {
  uint8_t meter;  // 0-based, meter 1 = 0
};

#endif
