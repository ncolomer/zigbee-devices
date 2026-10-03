#include "Sensor.h"
#include "Events.h"
#include "Debug.h"

#define SENSOR_BAUD         9600
#define FRAME_HEADER        0xFF
#define FRAME_LENGTH        4
#define MIN_VALID_MM        30       // below this the sensor is in its dead zone
#define POST_INTERVAL_MS    500
#define TIMEOUT_MS          10000

Sensor::Sensor(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin)
  : _serial(serial), _rxPin(rxPin), _txPin(txPin), _events(NULL) {}

void Sensor::begin(QueueHandle_t events) {
  _events = events;
  // TX idles high, which keeps the sensor in its filtered (stable) output mode
  _serial.begin(SENSOR_BAUD, SERIAL_8N1, _rxPin, _txPin);
  xTaskCreate(taskFunction, "sensor", 3072, this, 1, NULL);
}

void Sensor::taskFunction(void *parameter) {
  static_cast<Sensor *>(parameter)->runTask();
}

void Sensor::runTask() {
  uint8_t frame[FRAME_LENGTH];
  uint8_t len = 0;
  float distanceCm = NAN;
  uint32_t lastValid = millis();
  uint32_t lastPost = 0;
  bool fresh = false;

  for (;;) {
    while (_serial.available()) {
      uint8_t b = _serial.read();
      // Resync on the header byte; a bad frame just restarts the search
      if (len == 0 && b != FRAME_HEADER) continue;
      frame[len++] = b;
      if (len < FRAME_LENGTH) continue;
      len = 0;

      if (((frame[0] + frame[1] + frame[2]) & 0xFF) != frame[3]) {
        DEBUG_PRINTLN("Sensor: checksum error");
        continue;
      }
      uint16_t mm = (frame[1] << 8) | frame[2];
      if (mm <= MIN_VALID_MM) {
        DEBUG_PRINTLN("Sensor: below range (%u mm)", mm);
        continue;
      }
      distanceCm = mm / 10.0f;
      lastValid = millis();
      fresh = true;
    }

    uint32_t now = millis();
    if (now - lastPost >= POST_INTERVAL_MS) {
      bool timedOut = now - lastValid >= TIMEOUT_MS;
      if (fresh || timedOut) {
        Event e = { EventType::Distance, timedOut ? NAN : distanceCm };
        xQueueSend(_events, &e, 0);
        fresh = false;
      }
      lastPost = now;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
