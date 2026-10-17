#include "Sensor.h"
#include "Events.h"
#include "Debug.h"

#define SENSOR_BAUD         9600
#define FRAME_HEADER        0xFF
#define FRAME_LENGTH        4
#define MIN_VALID_MM        30       // below this the sensor is in its dead zone
#define POST_INTERVAL_MS    500
#define TIMEOUT_MS          10000
#define MEDIAN_WINDOW       11       // frames (100-500 ms each in processed mode): rejects bursts up to 5
#define MIN_SAMPLES         3        // the median needs this many to outvote one bad frame

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

// Ultrasonic echoes off walls or condensation give short bursts of wrong distances;
// a running median drops them while following the slow real level change.
static float median(const float *samples, uint8_t count) {
  float sorted[MEDIAN_WINDOW];
  memcpy(sorted, samples, count * sizeof(float));
  for (uint8_t i = 1; i < count; i++) {
    float v = sorted[i];
    int8_t j = i - 1;
    for (; j >= 0 && sorted[j] > v; j--) sorted[j + 1] = sorted[j];
    sorted[j + 1] = v;
  }
  return sorted[count / 2];
}

void Sensor::runTask() {
  uint8_t frame[FRAME_LENGTH];
  uint8_t len = 0;
  float filtered = NAN;
  uint32_t lastValid = millis();
  uint32_t lastPost = 0;
  bool fresh = false;
  float samples[MEDIAN_WINDOW];
  uint8_t count = 0, next = 0;

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
      float cm = mm / 10.0f;
      lastValid = millis();

      samples[next] = cm;
      next = (next + 1) % MEDIAN_WINDOW;
      if (count < MEDIAN_WINDOW) count++;
      filtered = median(samples, count);
      fresh = true;
      DEBUG_PRINTLN("Sensor: raw %.1f cm, median %.1f", cm, filtered);
    }

    uint32_t now = millis();
    if (now - lastPost >= POST_INTERVAL_MS) {
      if (now - lastValid >= TIMEOUT_MS) {
        count = next = 0;  // stale samples must not outvote the first ones after recovery
        Event e = { EventType::Distance, NAN };
        xQueueSend(_events, &e, 0);
      } else if (fresh && count >= MIN_SAMPLES) {  // a lone first frame could be a spike
        Event e = { EventType::Distance, filtered };
        xQueueSend(_events, &e, 0);
        fresh = false;
      }
      lastPost = now;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
