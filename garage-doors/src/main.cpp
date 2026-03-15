#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode is not selected"
#endif

#include "Zigbee.h"
#include "ZigbeeRelaySwitch.h"
#include "StatusLed.h"
#include "Debug.h"
#include <Preferences.h>

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "GarageDoors"

/* GPIO definitions — Grove Base headers, active-low relay */
#define RELAY_PIN_1   D1
#define RELAY_PIN_2   D2
#define RELAY_PIN_3   D4
#define RELAY_PIN_4   D6

#define LED_PIN       LED_BUILTIN

StatusLed statusLed(LED_PIN);
Preferences nvs;

ZigbeeRelaySwitch zbSwitch1(1, RELAY_PIN_1);
ZigbeeRelaySwitch zbSwitch2(2, RELAY_PIN_2);
ZigbeeRelaySwitch zbSwitch3(3, RELAY_PIN_3);
ZigbeeRelaySwitch zbSwitch4(4, RELAY_PIN_4);

ZigbeeRelaySwitch *switches[] = { &zbSwitch1, &zbSwitch2, &zbSwitch3, &zbSwitch4 };

uint16_t loadPulseDuration(uint8_t index) {
  char key[16];
  snprintf(key, sizeof(key), "pulse_ms_%d", index);
  nvs.begin("garage-doors", false);
  uint16_t ms = nvs.getUShort(key, 250); // default: 250ms
  nvs.end();
  return ms;
}

void savePulseDuration(uint8_t index, uint16_t ms) {
  char key[16];
  snprintf(key, sizeof(key), "pulse_ms_%d", index);
  nvs.begin("garage-doors", false);
  nvs.putUShort(key, ms);
  nvs.end();
  DEBUG_PRINTLN("Switch %d: pulse duration set to %u ms", index, ms);
}

uint8_t loadSwitchType(uint8_t index) {
  char key[16];
  snprintf(key, sizeof(key), "sw_type_%d", index);
  nvs.begin("garage-doors", false);
  uint8_t val = nvs.getUChar(key, 0);  // default: toggle
  nvs.end();
  return val;
}

void saveSwitchType(uint8_t index, uint8_t val) {
  char key[16];
  snprintf(key, sizeof(key), "sw_type_%d", index);
  nvs.begin("garage-doors", false);
  nvs.putUChar(key, val);
  nvs.end();
  DEBUG_PRINTLN("Switch %d: switchType set to %u", index, val);
}

uint8_t loadSwitchActions(uint8_t index) {
  char key[16];
  snprintf(key, sizeof(key), "sw_act_%d", index);
  nvs.begin("garage-doors", false);
  uint8_t val = nvs.getUChar(key, 0);  // default: active_high
  nvs.end();
  return val;
}

void saveSwitchActions(uint8_t index, uint8_t val) {
  char key[16];
  snprintf(key, sizeof(key), "sw_act_%d", index);
  nvs.begin("garage-doors", false);
  nvs.putUChar(key, val);
  nvs.end();
  DEBUG_PRINTLN("Switch %d: switchActions set to %u", index, val);
}

void connectZigbee() {
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();

  if (!Zigbee.begin(&zigbeeConfig, false)) {
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

void setup() {
  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  for (uint8_t i = 0; i < 4; i++) {
    uint8_t index = i + 1;
    uint16_t pulseDuration = loadPulseDuration(index);
    uint8_t switchType    = loadSwitchType(index);
    uint8_t switchActions = loadSwitchActions(index);

    switches[i]->setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
    switches[i]->setPowerSource(ZB_POWER_SOURCE_MAINS);
    switches[i]->allowMultipleBinding(true);
    switches[i]->setManualBinding(true);
    switches[i]->setDefaultPulseDuration(pulseDuration);
    switches[i]->setDefaultSwitchType(switchType);
    switches[i]->setDefaultSwitchActions(switchActions);

    switches[i]->onPulseDurationChanged([index](uint16_t ms) {
      savePulseDuration(index, ms);
    });
    switches[i]->onSwitchTypeChanged([index](uint8_t val) {
      saveSwitchType(index, val);
    });
    switches[i]->onSwitchActionsChanged([index](uint8_t val) {
      saveSwitchActions(index, val);
    });

    switches[i]->begin();

    Zigbee.addEndpoint(switches[i]);
    DEBUG_PRINTLN("Switch %d: pulse=%u ms type=%u actions=%u", index, pulseDuration, switchType, switchActions);
  }

  connectZigbee();
}

void loop() {
  delay(1000);
}
