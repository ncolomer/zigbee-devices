#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode is not selected"
#endif

#include "Zigbee.h"
#include "StatusLed.h"
#include "Debug.h"
#include "Events.h"
#include "Sensor.h"
#include "Display.h"
#include <Preferences.h>
#include "driver/gpio.h"

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "GarageWaterTank"

#define ENDPOINT_LEVEL          1
#define ENDPOINT_DISTANCE       2
#define ENDPOINT_MIN_DISTANCE   3
#define ENDPOINT_MAX_DISTANCE   4

/* GPIO definitions */
#define LED_PIN                 LED_BUILTIN   // activity LED
#define BUTTON_PIN              D0
#define SENSOR_RX_PIN           D7
#define SENSOR_TX_PIN           D6

/* Configuration */
#define DISPLAY_ON_MS           30000
#define DEBOUNCE_MS             50
#define LONG_PRESS_MS           2000
#define BLINK_MS                100
#define REPORT_MIN_S            60
#define REPORT_MAX_S            3600          // level can stay flat for hours; keeps the history alive
#define REPORT_DELTA            1
#define MARK_SAVE_STEP_PCT      1             // limits NVS writes while the level is rising

StatusLed statusLed(LED_PIN);
Preferences nvs;
Display display;
Sensor sensor(Serial1, SENSOR_RX_PIN, SENSOR_TX_PIN);

ZigbeeAnalog zbLevel(ENDPOINT_LEVEL);
ZigbeeAnalog zbDistance(ENDPOINT_DISTANCE);
ZigbeeAnalog zbMinDistance(ENDPOINT_MIN_DISTANCE);
ZigbeeAnalog zbMaxDistance(ENDPOINT_MAX_DISTANCE);

QueueHandle_t events;

float distanceCm = NAN;
float levelPct   = NAN;
float minCm      = NAN;   // distance when the tank is full
float maxCm      = NAN;   // distance when the tank is empty
float markPct    = NAN;   // high-water mark
float savedMarkPct = NAN;
float sentLevel  = NAN;
float sentDistance = NAN;

bool displayOn = false;
uint32_t displayOnAt = 0;
bool firstPairing = false;
bool joined = false;

/* Persistence */

void saveFloat(const char *key, float value) {
  nvs.begin("water-tank", false);
  nvs.putFloat(key, value);
  nvs.end();
}

void loadSettings() {
  nvs.begin("water-tank", true);
  minCm = nvs.getFloat("min_cm", NAN);
  maxCm = nvs.getFloat("max_cm", NAN);
  markPct = nvs.getFloat("mark_pct", NAN);
  nvs.end();
  savedMarkPct = markPct;
}

/* Button: the ISR only classifies the press, loop() acts on it */

volatile uint32_t pressedAt = 0;

void IRAM_ATTR onButton() {
  uint32_t now = millis();
  if (gpio_get_level((gpio_num_t)BUTTON_PIN) == 0) {
    pressedAt = now;
    return;
  }
  if (pressedAt == 0) return;  // released without a recorded press (held at boot)
  uint32_t held = now - pressedAt;
  pressedAt = 0;
  if (held < DEBOUNCE_MS) return;

  Event e = { held >= LONG_PRESS_MS ? EventType::ButtonLong : EventType::ButtonShort, 0 };
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(events, &e, &woken);
  if (woken) portYIELD_FROM_ISR();
}

/* Zigbee writes arrive on the Zigbee task; forward them instead of touching I2C/NVS here */

void postConfig(EventType type, float value) {
  Event e = { type, value };
  xQueueSend(events, &e, 0);
}

void onMinChange(float value) { postConfig(EventType::MinChanged, value); }
void onMaxChange(float value) { postConfig(EventType::MaxChanged, value); }

/* Application */

bool differs(float a, float b) {
  if (isnan(a) || isnan(b)) return isnan(a) != isnan(b);
  return fabsf(a - b) >= 0.05f;
}

void refreshDisplay() {
  if (!displayOn) return;
  if (isnan(levelPct)) display.showUnknown();
  else display.showLevel(levelPct, markPct);
}

void wakeDisplay() {
  display.setOn(true);
  displayOn = true;
  displayOnAt = millis();
  refreshDisplay();
}

void publish() {
  if (differs(levelPct, sentLevel)) {
    bool nanChanged = isnan(levelPct) != isnan(sentLevel);
    zbLevel.setAnalogInput(levelPct);
    if (nanChanged) zbLevel.reportAnalogInput();
    sentLevel = levelPct;
  }
  if (differs(distanceCm, sentDistance)) {
    bool nanChanged = isnan(distanceCm) != isnan(sentDistance);
    zbDistance.setAnalogInput(distanceCm);
    if (nanChanged) zbDistance.reportAnalogInput();
    sentDistance = distanceCm;
  }
}

void recompute() {
  float level = NAN;
  if (!isnan(distanceCm) && !isnan(minCm) && !isnan(maxCm) && maxCm > minCm) {
    level = constrain((maxCm - distanceCm) / (maxCm - minCm) * 100.0f, 0.0f, 100.0f);
  }

  if (!isnan(level) && (isnan(markPct) || level > markPct)) {
    markPct = level;
    if (isnan(savedMarkPct) || markPct - savedMarkPct >= MARK_SAVE_STEP_PCT) {
      saveFloat("mark_pct", markPct);
      savedMarkPct = markPct;
    }
  }

  bool levelChanged = differs(level, levelPct);
  levelPct = level;
  if (levelChanged) refreshDisplay();
  publish();
  DEBUG_PRINTLN("distance=%.1f cm level=%.1f%% mark=%.1f%%", distanceCm, levelPct, markPct);
}

void handleEvent(const Event &e) {
  switch (e.type) {
    case EventType::Distance:
      distanceCm = e.value;
      recompute();
      break;
    case EventType::MinChanged:
      minCm = e.value;
      saveFloat("min_cm", minCm);
      DEBUG_PRINTLN("min distance set to %.1f cm", minCm);
      statusLed.blink(BLINK_MS, 1.0, 1);
      recompute();
      break;
    case EventType::MaxChanged:
      maxCm = e.value;
      saveFloat("max_cm", maxCm);
      DEBUG_PRINTLN("max distance set to %.1f cm", maxCm);
      statusLed.blink(BLINK_MS, 1.0, 1);
      recompute();
      break;
    case EventType::ButtonShort:
      DEBUG_PRINTLN("button: short press");
      statusLed.blink(BLINK_MS, 1.0, 1);
      wakeDisplay();
      break;
    case EventType::ButtonLong:
      DEBUG_PRINTLN("button: long press, high-water mark reset");
      statusLed.blink(BLINK_MS, 1.0, 1);
      markPct = levelPct;
      saveFloat("mark_pct", markPct);
      savedMarkPct = markPct;
      wakeDisplay();
      break;
  }
}

void setupZigbee() {
  zbLevel.setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
  zbLevel.setPowerSource(ZB_POWER_SOURCE_MAINS);
  zbLevel.addAnalogInput();
  zbLevel.setAnalogInputApplication(ESP_ZB_ZCL_AI_PERCENTAGE_OTHER);
  zbLevel.setAnalogInputDescription("Water level (%)");
  zbLevel.setAnalogInputResolution(0.1);
  zbLevel.setAnalogInput(levelPct);

  zbDistance.addAnalogInput();
  zbDistance.setAnalogInputDescription("Distance (cm)");
  zbDistance.setAnalogInputResolution(0.1);
  zbDistance.setAnalogInput(distanceCm);

  zbMinDistance.addAnalogOutput();
  zbMinDistance.setAnalogOutputDescription("Min distance, tank full (cm)");
  zbMinDistance.setAnalogOutputResolution(0.1);
  zbMinDistance.setAnalogOutputMinMax(3, 600);
  zbMinDistance.onAnalogOutputChange(onMinChange);
  zbMinDistance.setAnalogOutput(minCm);

  zbMaxDistance.addAnalogOutput();
  zbMaxDistance.setAnalogOutputDescription("Max distance, tank empty (cm)");
  zbMaxDistance.setAnalogOutputResolution(0.1);
  zbMaxDistance.setAnalogOutputMinMax(3, 600);
  zbMaxDistance.onAnalogOutputChange(onMaxChange);
  zbMaxDistance.setAnalogOutput(maxCm);

  Zigbee.addEndpoint(&zbLevel);
  Zigbee.addEndpoint(&zbDistance);
  Zigbee.addEndpoint(&zbMinDistance);
  Zigbee.addEndpoint(&zbMaxDistance);

  // Shown until joined; the panel stays on and level draws are skipped meanwhile
  display.startPairing();
  wakeDisplay();

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  // Joining happens in the background, so display and sensor work meanwhile
  firstPairing = esp_zb_bdb_is_factory_new();
  if (firstPairing) statusLed.blink(2000, 0.5);
}

void checkJoined() {
  if (joined || !Zigbee.connected()) return;
  joined = true;
  display.stopPairing();
  wakeDisplay();

  // Reporting config needs a running stack, hence not in setupZigbee()
  zbLevel.setAnalogInputReporting(REPORT_MIN_S, REPORT_MAX_S, REPORT_DELTA);
  zbDistance.setAnalogInputReporting(REPORT_MIN_S, REPORT_MAX_S, REPORT_DELTA);

  if (firstPairing) {
    DEBUG_PRINTLN("joined network!");
    statusLed.blink(250, 0.2, 5);
  } else {
    DEBUG_PRINTLN("reconnected!");
  }
}

void setup() {
  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  events = xQueueCreate(16, sizeof(Event));
  loadSettings();
  DEBUG_PRINTLN("min=%.1f max=%.1f mark=%.1f", minCm, maxCm, markPct);

  statusLed.blink(BLINK_MS, 1.0, 1);

  if (!display.begin()) DEBUG_PRINTLN("Display not found");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButton, CHANGE);

  sensor.begin(events);
  setupZigbee();
}

void loop() {
  Event e;
  if (xQueueReceive(events, &e, pdMS_TO_TICKS(100))) handleEvent(e);

  if (displayOn && !display.isPairing() && millis() - displayOnAt >= DISPLAY_ON_MS) {
    display.setOn(false);
    displayOn = false;
  }

  checkJoined();
}
