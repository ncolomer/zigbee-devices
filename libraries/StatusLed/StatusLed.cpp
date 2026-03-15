#include "StatusLed.h"

StatusLed::StatusLed(uint8_t pin) : _pin(pin) {
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, HIGH); // LED is active LOW on XIAO ESP32C6, HIGH = OFF

  _commandQueue = xQueueCreate(1, sizeof(LedCommand));
  if (_commandQueue == NULL) {
    // Queue creation failed - this is critical
    return;
  }

  _stopSemaphore = xSemaphoreCreateBinary();
  if (_stopSemaphore == NULL) {
    // Semaphore creation failed
    return;
  }

  xTaskCreate(taskFunction, "StatusLed", 2048, this, 1, &_taskHandle);
}

StatusLed::~StatusLed() {
  if (_taskHandle != NULL) {
    vTaskDelete(_taskHandle);
  }
  if (_commandQueue != NULL) {
    vQueueDelete(_commandQueue);
  }
  if (_stopSemaphore != NULL) {
    vSemaphoreDelete(_stopSemaphore);
  }
}

void StatusLed::blink(uint32_t period, float duty, int8_t count) {
  if (_commandQueue == NULL) return;

  LedCommand cmd;
  cmd.type = LedCommandType::BLINK;
  cmd.parameters.period = period;
  cmd.parameters.duty = duty;
  cmd.parameters.count = count;

  // Send command, overwrite if queue is full (non-blocking)
  xQueueOverwrite(_commandQueue, &cmd);
}

void StatusLed::off() {
  if (_commandQueue == NULL) return;

  LedCommand cmd;
  cmd.type = LedCommandType::OFF;

  // Send command, overwrite if queue is full (non-blocking)
  xQueueOverwrite(_commandQueue, &cmd);
}

void StatusLed::on() {
  if (_commandQueue == NULL) return;

  LedCommand cmd;
  cmd.type = LedCommandType::ON;

  // Send command, overwrite if queue is full (non-blocking)
  xQueueOverwrite(_commandQueue, &cmd);
}

void StatusLed::stopAndWait(uint32_t timeoutMs) {
  if (_commandQueue == NULL || _stopSemaphore == NULL) return;

  LedCommand cmd;
  cmd.type = LedCommandType::STOP;

  // Send command, overwrite if queue is full (non-blocking)
  xQueueOverwrite(_commandQueue, &cmd);

  // Wait for task to finish
  xSemaphoreTake(_stopSemaphore, pdMS_TO_TICKS(timeoutMs));
}

void StatusLed::taskFunction(void* parameter) {
  StatusLed* instance = static_cast<StatusLed*>(parameter);
  instance->runTask();
}

void StatusLed::runTask() {
  LedCommand currentCmd;
  uint32_t lastToggle = 0;
  bool isBlinking = false;
  int8_t blinkRemaining = -1;
  bool stopping = false;

  while (!(stopping && blinkRemaining == -1)) {
    if (xQueueReceive(_commandQueue, &currentCmd, 0)) {
      switch (currentCmd.type) {
        case LedCommandType::OFF:
          digitalWrite(_pin, HIGH);
          blinkRemaining = -1;
          isBlinking = false;
          break;
        case LedCommandType::ON:
          digitalWrite(_pin, LOW);
          blinkRemaining = -1;
          isBlinking = false;
          break;
        case LedCommandType::STOP:
          stopping = true;
          break;
        case LedCommandType::BLINK:
          blinkRemaining = currentCmd.parameters.count;
          digitalWrite(_pin, HIGH);
          isBlinking = true;
          lastToggle = millis();
          break;
      }
      continue;
    }

    if (blinkRemaining == -1 && !isBlinking) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    uint32_t now = millis();
    uint32_t onTime = (uint32_t)(currentCmd.parameters.period * currentCmd.parameters.duty);
    uint32_t elapsed = now - lastToggle;

    if (digitalRead(_pin) != LOW && elapsed >= (currentCmd.parameters.period - onTime)) {
      digitalWrite(_pin, LOW);
      lastToggle = now;
    } else if (digitalRead(_pin) == LOW && elapsed >= onTime) {
      digitalWrite(_pin, HIGH);
      lastToggle = now;
      if (blinkRemaining > 0 && --blinkRemaining == 0) {
        blinkRemaining = -1;
        isBlinking = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (_stopSemaphore != NULL) {
    xSemaphoreGive(_stopSemaphore);
  }

  vTaskDelete(NULL);
}
