#ifndef ZIGBEE_MODE_ZCZR
  #error "Zigbee coordinator/router device mode is not selected"
#endif

#include "Zigbee.h"
#include "ZigbeeRelaySwitch.h"
#include "ZigbeeWaterMeter.h"
#include "StatusLed.h"
#include "Debug.h"
#include "Events.h"
#include "Meters.h"
#include <Preferences.h>

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "GarageWater"

/* Endpoints */
#define EP_RELAY                1
#define EP_METER_1              2
#define EP_METER_2              3

/* GPIO definitions — XIAO ESP32-C6 + Grove Shield */
#define RELAY_PIN               D4           // ON/grid = LOW (energized), OFF/tank = HIGH (de-energized)
#define BUTTON_PIN              BOOT_PIN     // GPIO9 onboard boot button (factory reset)
#define LED_PIN                 LED_BUILTIN

/* Configuration */
#define RELAY_SWITCH_ACTIONS       1         // active_low: ON(grid)=LOW, OFF(tank)=HIGH
#define DEFAULT_RELAY_STATE        false     // tank by default (fail-safe on a fresh device)
#define DEFAULT_LITERS_PER_PULSE   10        // per meter, calibratable over Zigbee
#define SAVE_READING_THRESHOLD_L   1000u     // persist a meter reading every 1 m³
#define FACTORY_RESET_TIME_MS      3000      // hold BOOT this long to factory-reset

StatusLed statusLed(LED_PIN);
Preferences nvs;
Meters meterInputs;
QueueHandle_t events;

ZigbeeRelaySwitch zbRelay(EP_RELAY, RELAY_PIN);
ZigbeeWaterMeter  zbMeter1(EP_METER_1);
ZigbeeWaterMeter  zbMeter2(EP_METER_2);

struct Meter {
  ZigbeeWaterMeter *zb;
  const char *reading_key;
  const char *lpp_key;
  uint32_t liters;             // current reading
  uint32_t nvs_liters;         // last value persisted
  uint16_t liters_per_pulse;
};

Meter meters[METERS] = {
  { &zbMeter1, "m1_reading", "m1_lpp", 0, 0, 0 },
  { &zbMeter2, "m2_reading", "m2_lpp", 0, 0, 0 },
};

bool firstPairing = false;
bool joined = false;
uint32_t resetPressedAt = 0;

bool loadRelayState() {
  nvs.begin("garage-water", false);
  // Key renamed from "relay" when ON changed from tank to grid: an old value would invert the source
  bool state = nvs.getBool("grid", DEFAULT_RELAY_STATE);
  nvs.end();
  return state;
}

void saveRelayState(bool state) {
  nvs.begin("garage-water", false);
  nvs.putBool("grid", state);
  nvs.end();
  DEBUG_PRINTLN("Saved relay state: %s", state ? "ON (grid)" : "OFF (tank)");
}

void loadMeter(Meter &m) {
  nvs.begin("garage-water", false);
  m.liters = nvs.getUInt(m.reading_key, 0);
  m.liters_per_pulse = nvs.getUShort(m.lpp_key, DEFAULT_LITERS_PER_PULSE);
  nvs.end();
  m.nvs_liters = m.liters;
}

void saveMeterReading(Meter &m, bool force = false) {
  if (force || m.liters - m.nvs_liters >= SAVE_READING_THRESHOLD_L) {
    nvs.begin("garage-water", false);
    nvs.putUInt(m.reading_key, m.liters);
    nvs.end();
    m.nvs_liters = m.liters;
    DEBUG_PRINTLN("Saved %s: %u L", m.reading_key, m.liters);
  }
}

void setupZigbee() {
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  // Joining happens in the background so the pulse inputs keep being sampled
  firstPairing = esp_zb_bdb_is_factory_new();
  if (firstPairing) statusLed.blink(2000, 0.5);
}

void checkJoined() {
  if (joined || !Zigbee.connected()) return;
  joined = true;

  if (firstPairing) {
    DEBUG_PRINTLN("joined network!");
    statusLed.blink(250, 0.2, 5);
  } else {
    DEBUG_PRINTLN("reconnected!");
  }
}

void setupRelay() {
  bool state = loadRelayState();
  zbRelay.setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
  zbRelay.setPowerSource(ZB_POWER_SOURCE_MAINS);
  zbRelay.setDefaultSwitchType(0);                        // toggle / latching
  zbRelay.setDefaultSwitchActions(RELAY_SWITCH_ACTIONS);  // active_low
  zbRelay.setDefaultOnOff(state);                         // restore persisted state
  zbRelay.onStateChanged([](bool on) {
    saveRelayState(on);
  });
  zbRelay.begin();                                        // drives GPIO to restored state
  Zigbee.addEndpoint(&zbRelay);
  DEBUG_PRINTLN("Relay: restored %s", state ? "ON (grid)" : "OFF (tank)");
}

void setupMeters() {
  for (uint8_t i = 0; i < METERS; i++) {
    Meter &m = meters[i];
    uint8_t idx = i;

    loadMeter(m);

    m.zb->setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
    m.zb->setPowerSource(ZB_POWER_SOURCE_MAINS);
    m.zb->setDefaultLitersPerPulse(m.liters_per_pulse);
    m.zb->setDefaultReadingLiters(m.liters);

    m.zb->onWaterVolumeChanged([idx](uint32_t liters) {
      Meter &mm = meters[idx];
      liters = min(liters, (uint32_t)99999999u);
      mm.liters = liters;
      mm.nvs_liters = liters;
      nvs.begin("garage-water", false);
      nvs.putUInt(mm.reading_key, liters);
      nvs.end();
      DEBUG_PRINTLN("User set %s = %u L (%.3f m3)", mm.reading_key, liters, liters / 1000.0f);
    });
    m.zb->onLitersPerPulseChanged([idx](uint16_t value) {
      Meter &mm = meters[idx];
      value = constrain(value, 1, 1000);
      mm.liters_per_pulse = value;
      nvs.begin("garage-water", false);
      nvs.putUShort(mm.lpp_key, value);
      nvs.end();
      DEBUG_PRINTLN("%s liters_per_pulse = %u", mm.lpp_key, value);
    });

    Zigbee.addEndpoint(m.zb);

    DEBUG_PRINTLN("Meter %d: reading=%u L, l/pulse=%u", idx + 1, m.liters, m.liters_per_pulse);
  }
}

void checkFactoryReset() {
  if (digitalRead(BUTTON_PIN) == HIGH) {
    resetPressedAt = 0;
    return;
  }
  if (resetPressedAt == 0) resetPressedAt = millis();
  if (millis() - resetPressedAt >= FACTORY_RESET_TIME_MS) {
    DEBUG_PRINTLN("Factory reset (leaving Zigbee network)");
    statusLed.blink(200, 0.5, 3);
    for (uint8_t i = 0; i < METERS; i++) saveMeterReading(meters[i], true);
    DEBUG_END();
    Zigbee.factoryReset();
    ESP.restart();
  }
}

void handlePulse(const PulseEvent &e) {
  Meter &m = meters[e.meter];
  m.liters = min(m.liters + m.liters_per_pulse, (uint32_t)99999999u);
  m.zb->setReadingLiters(m.liters);
  saveMeterReading(m);
  DEBUG_PRINTLN("Meter %d: +1 pulse -> %u L (%.3f m3)", e.meter + 1, m.liters, m.liters / 1000.0f);
}

void setup() {
  // Hold the relay de-energized (= tank, safe default) until ZigbeeRelaySwitch::begin() drives the pin.
  pinMode(RELAY_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  events = xQueueCreate(16, sizeof(PulseEvent));
  setupRelay();
  setupMeters();
  meterInputs.begin(events);
  setupZigbee();
}

void loop() {
  PulseEvent e;
  if (xQueueReceive(events, &e, pdMS_TO_TICKS(100))) handlePulse(e);

  checkFactoryReset();
  checkJoined();
}
