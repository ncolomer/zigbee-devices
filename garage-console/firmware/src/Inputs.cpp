#include "Inputs.h"
#include "Events.h"
#include "Debug.h"
#include <Wire.h>

#define PCF_ADDR      0x20
#define PCF_SDA_PIN   22
#define PCF_SCL_PIN   23
#define PCF_INT_PIN   2
#define DEBOUNCE_MS   30
#define POLL_MS       1000

uint16_t Inputs::read() {
  Wire.requestFrom(PCF_ADDR, 2);
  uint16_t raw = Wire.read();
  raw |= (uint16_t)Wire.read() << 8;
  return ~raw;
}

void IRAM_ATTR Inputs::onInterrupt(void *arg) {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(((Inputs *)arg)->_task, &woken);
  if (woken) portYIELD_FROM_ISR();
}

void Inputs::begin(QueueHandle_t events) {
  _events = events;

  Wire.begin(PCF_SDA_PIN, PCF_SCL_PIN);
  Wire.beginTransmission(PCF_ADDR);  // writing 0xFFFF is the input configuration
  Wire.write(0xFF);
  Wire.write(0xFF);
  if (Wire.endTransmission() != 0) DEBUG_PRINTLN("PCF8575 not found");
  _state = read();

  xTaskCreate(taskFunction, "inputs", 3072, this, 5, &_task);
  pinMode(PCF_INT_PIN, INPUT);  // R3 pulls it up
  attachInterruptArg(PCF_INT_PIN, onInterrupt, this, FALLING);
}

void Inputs::taskFunction(void *arg) {
  ((Inputs *)arg)->runTask();
}

void Inputs::runTask() {
  for (;;) {
    // The poll catches an edge lost while the expander was being read (PCF8575 datasheet 8.3.2)
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_MS));

    uint16_t now = read();  // also clears INT
    if (now == _state) continue;

    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
    uint16_t settled = read();
    uint16_t changed = (now ^ _state) & ~(now ^ settled);  // bits still at the new level

    for (uint8_t ch = 0; ch < CHANNELS; ch++) {
      if (!(changed & (1 << ch))) continue;
      _state ^= (1 << ch);
      InputEvent e = { ch, (bool)(_state & (1 << ch)) };
      xQueueSend(_events, &e, 0);
    }
  }
}
