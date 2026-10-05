#include "Meters.h"
#include "Events.h"
#include "Debug.h"

#define METER1_PIN     D1
#define METER2_PIN     D2
#define CLOSE_HOLD_US  100000u   // line must sit LOW this long (no edge) to confirm contact closed
#define OPEN_HOLD_US   500000u   // ...and HIGH this long to re-arm; absorbs slow chatter at the switching point
#define SETTLE_POLL_MS 10        // re-check rate while a change waits for its hold time

void IRAM_ATTR Meters::onEdge(void *arg) {
  Line *l = (Line *)arg;
  l->last_us = micros();
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(l->task, &woken);
  if (woken) portYIELD_FROM_ISR();
}

void Meters::begin(QueueHandle_t events) {
  _events = events;
  const uint8_t pins[METERS] = { METER1_PIN, METER2_PIN };
  for (uint8_t i = 0; i < METERS; i++) {
    Line &l = _lines[i];
    l.pin = pins[i];
    l.last_us = micros();
    pinMode(l.pin, INPUT_PULLUP);
    l.closed = digitalRead(l.pin) == LOW;  // no count for a contact already closed at boot
  }

  TaskHandle_t task;
  xTaskCreate(taskFunction, "meters", 3072, this, 5, &task);
  for (uint8_t i = 0; i < METERS; i++) {
    _lines[i].task = task;
    attachInterruptArg(digitalPinToInterrupt(_lines[i].pin), onEdge, &_lines[i], CHANGE);
  }
}

void Meters::taskFunction(void *arg) {
  ((Meters *)arg)->runTask();
}

// One pulse per confirmed open -> closed transition. A closed contact must then stay HIGH
// for OPEN_HOLD_US before the next pulse can count, so bounce or a contact dithering at its
// switching point (low flow) counts once, and a contact stuck closed never recounts.
void Meters::runTask() {
  for (;;) {
    bool pending = false;
    for (uint8_t i = 0; i < METERS; i++) {
      Line &l = _lines[i];
      bool low = digitalRead(l.pin) == LOW;  // read level before last_us: a newer edge then only delays confirmation
      uint32_t last = l.last_us;
      uint32_t quiet_us = micros() - last;

      if (!l.closed && low && quiet_us >= CLOSE_HOLD_US) {
        l.closed = true;
        PulseEvent e = { i };
        xQueueSend(_events, &e, 0);
      } else if (l.closed && !low && quiet_us >= OPEN_HOLD_US) {
        l.closed = false;
      }
      if (low != l.closed) pending = true;
    }
    ulTaskNotifyTake(pdTRUE, pending ? pdMS_TO_TICKS(SETTLE_POLL_MS) : portMAX_DELAY);
  }
}
