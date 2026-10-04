#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// A02YYUW ultrasonic sensor, UART output. Posts EventType::Distance to the given queue.
class Sensor {
public:
  Sensor(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin);
  void begin(QueueHandle_t events);

private:
  HardwareSerial &_serial;
  uint8_t _rxPin, _txPin;
  QueueHandle_t _events;

  static void taskFunction(void *parameter);
  void runTask();
};

#endif
