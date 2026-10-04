#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode is not selected"
#endif

#include "Zigbee.h"
#include "ZigbeeSwitchInput.h"
#include "StatusLed.h"
#include "Debug.h"
#include "Events.h"
#include "Inputs.h"
#include <Preferences.h>

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "GarageConsole"

/* GPIO definitions */
#define LED_PIN                 LED_BUILTIN
#define RESET_PIN               BOOT_PIN      // XIAO BOOT button, GPIO9

/* Configuration */
#define BINDING_TABLE_SIZE      64
#define BLINK_MS                100
#define FACTORY_RESET_MS        3000

StatusLed statusLed(LED_PIN);
Preferences nvs;
Inputs inputs;
QueueHandle_t events;

// Endpoint N is connector JN
ZigbeeSwitchInput zbInputs[CHANNELS] = {
  ZigbeeSwitchInput(1),  ZigbeeSwitchInput(2),  ZigbeeSwitchInput(3),  ZigbeeSwitchInput(4),
  ZigbeeSwitchInput(5),  ZigbeeSwitchInput(6),  ZigbeeSwitchInput(7),  ZigbeeSwitchInput(8),
  ZigbeeSwitchInput(9),  ZigbeeSwitchInput(10), ZigbeeSwitchInput(11), ZigbeeSwitchInput(12),
  ZigbeeSwitchInput(13), ZigbeeSwitchInput(14), ZigbeeSwitchInput(15), ZigbeeSwitchInput(16),
};

bool firstPairing = false;
bool joined = false;
uint32_t resetPressedAt = 0;

/* Persistence */

uint8_t loadSwitchType(uint8_t index) {
  char key[16];
  snprintf(key, sizeof(key), "sw_type_%d", index);
  nvs.begin("garage-console", false);  // read-only fails while the namespace doesn't exist yet
  uint8_t val = nvs.getUChar(key, ZigbeeSwitchInput::SWITCH_MOMENTARY);
  nvs.end();
  return val;
}

void saveSwitchType(uint8_t index, uint8_t val) {
  char key[16];
  snprintf(key, sizeof(key), "sw_type_%d", index);
  nvs.begin("garage-console", false);
  nvs.putUChar(key, val);
  nvs.end();
  DEBUG_PRINTLN("J%d: switchType set to %u", index, val);
}

/* Application */

void handleInput(const InputEvent &e) {
  ZigbeeSwitchInput &zb = zbInputs[e.channel];
  DEBUG_PRINTLN("J%d: %s", e.channel + 1, e.closed ? "closed" : "open");
  if (!joined) return;

  if (zb.switchType() == ZigbeeSwitchInput::SWITCH_TOGGLE) {
    zb.setState(e.closed);
    e.closed ? zb.sendOn() : zb.sendOff();
  } else if (e.closed) {
    zb.sendToggle();
  } else {
    return;
  }
  statusLed.blink(BLINK_MS, 1.0, 1);
}

void setupZigbee() {
  for (uint8_t i = 0; i < CHANNELS; i++) {
    uint8_t index = i + 1;
    uint8_t switchType = loadSwitchType(index);

    if (i == 0) {
      zbInputs[i].setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
      zbInputs[i].setPowerSource(ZB_POWER_SOURCE_MAINS);
    }
    zbInputs[i].allowMultipleBinding(true);
    zbInputs[i].setManualBinding(true);
    zbInputs[i].setDefaultSwitchType(switchType);
    zbInputs[i].onSwitchTypeChanged([index](uint8_t val) { saveSwitchType(index, val); });

    Zigbee.addEndpoint(&zbInputs[i]);
    DEBUG_PRINTLN("J%d: switchType=%u", index, switchType);
  }

  Zigbee.setRxOnWhenIdle(true);  // mains-powered, must receive bind/config requests right away

  // Z2M binds each endpoint to the coordinator (16 entries), then every user binding adds more;
  // the default table (16) is full right after pairing. Only effective before the stack starts.
  esp_err_t bindRet = esp_zb_aps_src_binding_table_size_set(BINDING_TABLE_SIZE);
  DEBUG_PRINTLN("binding table size %d: %s", BINDING_TABLE_SIZE, esp_err_to_name(bindRet));

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  // Joining happens in the background so inputs keep being sampled
  firstPairing = esp_zb_bdb_is_factory_new();
  if (firstPairing) statusLed.blink(2000, 0.5);
}

void checkJoined() {
  if (joined || !Zigbee.connected()) return;
  joined = true;

  // Toggle channels report the position they boot in, without sending a command
  uint16_t state = inputs.state();
  for (uint8_t i = 0; i < CHANNELS; i++) {
    if (zbInputs[i].switchType() == ZigbeeSwitchInput::SWITCH_TOGGLE) zbInputs[i].setState(state & (1 << i));
  }

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

// Only the physical switches own the state: put it back if the coordinator wrote to it
void restoreOverriddenStates() {
  uint16_t state = inputs.state();
  for (uint8_t i = 0; i < CHANNELS; i++) {
    if (zbInputs[i].takeStateOverridden()) zbInputs[i].setState(state & (1 << i));
  }
}

void setup() {
  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  events = xQueueCreate(32, sizeof(InputEvent));
  pinMode(RESET_PIN, INPUT_PULLUP);

  statusLed.blink(BLINK_MS, 1.0, 1);
  inputs.begin(events);
  setupZigbee();
}

void loop() {
  InputEvent e;
  if (xQueueReceive(events, &e, pdMS_TO_TICKS(100))) handleInput(e);

  restoreOverriddenStates();
  checkFactoryReset();
  checkJoined();
}
