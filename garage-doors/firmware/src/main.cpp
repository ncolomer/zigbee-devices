#ifndef ZIGBEE_MODE_ZCZR
  #error "Zigbee coordinator/router device mode is not selected"
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
#define RELAY_PIN_1             D1
#define RELAY_PIN_2             D2
#define RELAY_PIN_3             D4
#define RELAY_PIN_4             D6
#define LED_PIN                 LED_BUILTIN
#define RESET_PIN               BOOT_PIN      // XIAO BOOT button, GPIO9

/* Configuration */
#define CHANNELS                4
#define DEFAULT_PULSE_MS        250
#define DEFAULT_SWITCH_TYPE     0             // toggle
#define DEFAULT_SWITCH_ACTIONS  0             // active_high
#define FACTORY_RESET_MS        3000

StatusLed statusLed(LED_PIN);
Preferences nvs;

// Endpoint N drives relay N
ZigbeeRelaySwitch zbSwitches[CHANNELS] = {
  ZigbeeRelaySwitch(1, RELAY_PIN_1), ZigbeeRelaySwitch(2, RELAY_PIN_2),
  ZigbeeRelaySwitch(3, RELAY_PIN_3), ZigbeeRelaySwitch(4, RELAY_PIN_4),
};

bool firstPairing = false;
bool joined = false;
uint32_t resetPressedAt = 0;

/* Persistence */

uint16_t loadPulseDuration(uint8_t index) {
  char key[16];
  snprintf(key, sizeof(key), "pulse_ms_%d", index);
  nvs.begin("garage-doors", false);  // read-only fails while the namespace doesn't exist yet
  uint16_t ms = nvs.getUShort(key, DEFAULT_PULSE_MS);
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
  uint8_t val = nvs.getUChar(key, DEFAULT_SWITCH_TYPE);
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
  uint8_t val = nvs.getUChar(key, DEFAULT_SWITCH_ACTIONS);
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

/* Application */

void setupSwitches() {
  for (uint8_t i = 0; i < CHANNELS; i++) {
    ZigbeeRelaySwitch &sw = zbSwitches[i];
    uint8_t index = i + 1;
    uint16_t pulseDuration = loadPulseDuration(index);
    uint8_t switchType    = loadSwitchType(index);
    uint8_t switchActions = loadSwitchActions(index);

    sw.setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
    sw.setPowerSource(ZB_POWER_SOURCE_MAINS);
    sw.allowMultipleBinding(true);
    sw.setManualBinding(true);
    sw.setDefaultPulseDuration(pulseDuration);
    sw.setDefaultSwitchType(switchType);
    sw.setDefaultSwitchActions(switchActions);

    sw.onPulseDurationChanged([index](uint16_t ms) { savePulseDuration(index, ms); });
    sw.onSwitchTypeChanged([index](uint8_t val) { saveSwitchType(index, val); });
    sw.onSwitchActionsChanged([index](uint8_t val) { saveSwitchActions(index, val); });

    sw.begin();  // drives the relay pin to its idle level
    Zigbee.addEndpoint(&sw);
    DEBUG_PRINTLN("Switch %d: pulse=%u ms type=%u actions=%u", index, pulseDuration, switchType, switchActions);
  }
}

void setupZigbee() {
  // Router rather than end device: an end device that loses its parent never rejoins on this stack
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  // Joining happens in the background so the reset button keeps working
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

void checkFactoryReset() {
  if (digitalRead(RESET_PIN) == HIGH) {
    resetPressedAt = 0;
    return;
  }
  if (resetPressedAt == 0) resetPressedAt = millis();
  if (millis() - resetPressedAt >= FACTORY_RESET_MS) {
    DEBUG_PRINTLN("factory reset");
    statusLed.blink(250, 0.5, 5);
    Zigbee.factoryReset();
  }
}

void setup() {
  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  pinMode(RESET_PIN, INPUT_PULLUP);

  setupSwitches();
  setupZigbee();
}

void loop() {
  checkFactoryReset();
  checkJoined();
  delay(100);
}
