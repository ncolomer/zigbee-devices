#ifndef ZIGBEE_MODE_ZCZR
  #error "Zigbee coordinator/router device mode is not selected"
#endif

#include "Zigbee.h"
#include "ZigbeeRelaySwitch.h"
#include "ZigbeeWaterMeter.h"
#include "StatusLed.h"
#include "Debug.h"
#include <Preferences.h>

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "GarageWater"

/* Endpoints */
#define EP_RELAY                1
#define EP_METER_1              2
#define EP_METER_2              3

/* GPIO definitions — XIAO ESP32-C6 + Grove Shield */
#define RELAY_PIN               D4           // ON/tank = HIGH (de-energized), OFF/grid = LOW (energized)
#define METER1_PIN              D1           // water meter 1 pulse
#define METER2_PIN              D2           // water meter 2 pulse
#define BUTTON_PIN              BOOT_PIN     // GPIO9 onboard boot button (factory reset)
#define LED_PIN                 LED_BUILTIN

/* Configuration */
#define RELAY_SWITCH_ACTIONS       0         // active_high: ON(tank)=HIGH, OFF(grid)=LOW
#define DEFAULT_RELAY_STATE        true      // tank by default (fail-safe on a fresh device)
#define DEFAULT_LITERS_PER_PULSE   10        // per meter, calibratable over Zigbee
#define SAVE_READING_THRESHOLD_L   1000u     // persist a meter reading every 1 m³
#define PULSE_DEBOUNCE_US          10000u    // 10 ms, matches the ptvo counters
#define FACTORY_RESET_TIME_MS      3000      // hold BOOT this long to factory-reset

StatusLed statusLed(LED_PIN);
Preferences nvs;

ZigbeeRelaySwitch zbRelay(EP_RELAY, RELAY_PIN);
ZigbeeWaterMeter  zbMeter1(EP_METER_1);
ZigbeeWaterMeter  zbMeter2(EP_METER_2);

struct Meter {
  ZigbeeWaterMeter *zb;
  uint8_t pin;
  const char *reading_key;
  const char *lpp_key;
  volatile uint32_t pulses;    // incremented in ISR
  volatile uint32_t last_us;   // debounce timestamp
  uint32_t counted;            // pulses already applied in loop()
  uint32_t liters;             // current reading
  uint32_t nvs_liters;         // last value persisted
  uint16_t liters_per_pulse;
};

Meter meters[2] = {
  { &zbMeter1, METER1_PIN, "m1_reading", "m1_lpp", 0, 0, 0, 0, 0, 0 },
  { &zbMeter2, METER2_PIN, "m2_reading", "m2_lpp", 0, 0, 0, 0, 0, 0 },
};

void IRAM_ATTR onMeter1Pulse() {
  uint32_t now = micros();
  if (now - meters[0].last_us < PULSE_DEBOUNCE_US) return;
  meters[0].last_us = now;
  meters[0].pulses = meters[0].pulses + 1;
}

void IRAM_ATTR onMeter2Pulse() {
  uint32_t now = micros();
  if (now - meters[1].last_us < PULSE_DEBOUNCE_US) return;
  meters[1].last_us = now;
  meters[1].pulses = meters[1].pulses + 1;
}

bool loadRelayState() {
  nvs.begin("garage-water", false);
  bool state = nvs.getBool("relay", DEFAULT_RELAY_STATE);
  nvs.end();
  return state;
}

void saveRelayState(bool state) {
  nvs.begin("garage-water", false);
  nvs.putBool("relay", state);
  nvs.end();
  DEBUG_PRINTLN("Saved relay state: %s", state ? "ON (tank)" : "OFF (grid)");
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

void connectZigbee() {
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  bool is_first_pairing = esp_zb_bdb_is_factory_new();
  if (is_first_pairing) statusLed.blink(2000, 0.5);

  DEBUG_PRINT("Connecting to Zigbee network ");
  uint32_t last_second = 0;
  while (!Zigbee.connected()) {
    uint32_t current_second = millis() / 1000;
    if (current_second != last_second) {
      DEBUG_PRINT(".");
      last_second = current_second;
    }
    delay(50);
  }

  if (is_first_pairing) {
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
  zbRelay.setDefaultSwitchActions(RELAY_SWITCH_ACTIONS);  // active_high
  zbRelay.setDefaultOnOff(state);                         // restore persisted state
  zbRelay.onStateChanged([](bool on) {
    saveRelayState(on);
  });
  zbRelay.begin();                                        // drives GPIO to restored state
  Zigbee.addEndpoint(&zbRelay);
  DEBUG_PRINTLN("Relay: restored %s", state ? "ON (tank)" : "OFF (grid)");
}

void setupMeters() {
  for (uint8_t i = 0; i < 2; i++) {
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

    pinMode(m.pin, INPUT_PULLUP);
    DEBUG_PRINTLN("Meter %d: reading=%u L, l/pulse=%u", idx + 1, m.liters, m.liters_per_pulse);
  }

  attachInterrupt(digitalPinToInterrupt(METER1_PIN), onMeter1Pulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(METER2_PIN), onMeter2Pulse, FALLING);
}

void checkFactoryReset() {
  if (digitalRead(BUTTON_PIN) != LOW) return;

  uint32_t start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - start > FACTORY_RESET_TIME_MS) {
      DEBUG_PRINTLN("Factory reset (leaving Zigbee network)");
      statusLed.blink(200, 0.5, 3);
      for (uint8_t i = 0; i < 2; i++) saveMeterReading(meters[i], true);
      DEBUG_END();
      Zigbee.factoryReset();
      ESP.restart();
    }
    delay(50);
  }
}

void processMeters() {
  for (uint8_t i = 0; i < 2; i++) {
    Meter &m = meters[i];
    uint32_t pulses = m.pulses;  // atomic 32-bit read
    if (pulses == m.counted) continue;

    uint32_t delta = pulses - m.counted;
    m.counted = pulses;
    m.liters = min(m.liters + delta * m.liters_per_pulse, (uint32_t)99999999u);
    m.zb->setReadingLiters(m.liters);
    saveMeterReading(m);
    DEBUG_PRINTLN("Meter %d: +%u pulse(s) -> %u L (%.3f m3)", i + 1, delta, m.liters, m.liters / 1000.0f);
  }
}

void setup() {
  // Hold the relay de-energized (= tank, safe default) until ZigbeeRelaySwitch::begin() drives the pin.
  pinMode(RELAY_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT);

  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  setupRelay();
  setupMeters();
  connectZigbee();
}

void loop() {
  checkFactoryReset();
  processMeters();
  delay(50);
}
