#ifndef EVENTS_H
#define EVENTS_H

#include <Arduino.h>

// Everything that can wake the main loop; loop() is the only consumer.
enum class EventType : uint8_t {
  ButtonShort,
  ButtonLong,
  Distance,   // value = cm, NaN on sensor timeout
  MinChanged, // value = cm, written from Zigbee
  MaxChanged
};

struct Event {
  EventType type;
  float value;
};

#endif
