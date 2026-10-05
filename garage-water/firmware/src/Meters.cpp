#include "Meters.h"
#include "Events.h"
#include "Debug.h"

#define METER1_PIN     D1
#define METER2_PIN     D2
#define REARM_HOLD_US  500000u   // line must sit HIGH this long (no edge) to re-arm; absorbs chatter at the switching point
#define SETTLE_POLL_MS 10        // re-check rate while waiting for the re-arm hold time

void IRAM_ATTR Meters::onEdge(void *arg) {
  Line *l = (Line *)arg;
  l->last_us = micros();
  l->edges = l->edges + 1;
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
    l.edges = 0;
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

// Counts on the first LOW seen while armed, then ignores the line until it has stayed HIGH for
// REARM_HOLD_US. Bounce, chatter and a contact parked closed (low flow) count once; waiting for a
// quiet LOW instead would miss pulses whenever the line keeps chattering while the contact is closed.
void Meters::runTask() {
  for (;;) {
    bool pending = false;
    for (uint8_t i = 0; i < METERS; i++) {
      Line &l = _lines[i];
      bool low = digitalRead(l.pin) == LOW;  // read level before last_us: a newer edge then only delays re-arming
      uint32_t last = l.last_us;
      uint32_t quiet_us = micros() - last;

      if (!l.closed && low) {
        l.closed = true;
        l.edges = 0;
        PulseEvent e = { i };
        xQueueSend(_events, &e, 0);
      } else if (l.closed && !low && quiet_us >= REARM_HOLD_US) {
        l.closed = false;
        DEBUG_PRINTLN("Meter %d: re-armed after %u edges", i + 1, l.edges);
      }
      if (l.closed && !low) pending = true;
    }
    ulTaskNotifyTake(pdTRUE, pending ? pdMS_TO_TICKS(SETTLE_POLL_MS) : portMAX_DELAY);
  }
}
