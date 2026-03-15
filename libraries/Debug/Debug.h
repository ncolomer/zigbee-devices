#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

#ifdef DEBUG
  #define USE_SERIAL
  #define ENABLE_SERIAL_DEBUG 1
#else
  #define ENABLE_SERIAL_DEBUG 0
#endif

#if ENABLE_SERIAL_DEBUG
  #define DEBUG_INIT()        Serial.begin(115200); for(uint32_t t=millis(); !Serial && (millis()-t)<2000; delay(50)); delay(200);
  #define DEBUG_PRINT(...)    Serial.printf(__VA_ARGS__)
  #define DEBUG_PRINTLN(...)  Serial.printf(__VA_ARGS__); Serial.println()
  #define DEBUG_END()         Serial.flush(); Serial.end();
#else
  #define DEBUG_INIT()
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
  #define DEBUG_END()
#endif

#endif // DEBUG_H
