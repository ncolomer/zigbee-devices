#ifndef EVENTS_H
#define EVENTS_H

#include <Arduino.h>

// Everything that can wake the main loop; loop() is the only consumer.
struct InputEvent {
  uint8_t channel;  // 0-based, J1 = 0
  bool closed;
};

#endif
