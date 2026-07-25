#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include "ZigbeeContactSensor.h"
#include "StatusLed.h"
#include "Debug.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "ContactSensor"

#define ENDPOINT_REED_RELAY     1

/* GPIO definitions */
#define BUTTON_PIN              BOOT_PIN             // Boot button for factory reset
#define LED_PIN                 LED_BUILTIN          // Built-in LED
#define REED_PIN                2                    // GPIO2 / D2 / A2 / LP_GPIO2
#define REED_PIN_BITMASK        (1ULL << REED_PIN)

/* Factory reset configuration */
#define FACTORY_RESET_TIME_MS    3000                 // Factory reset button hold time in milliseconds

// Status LED (initialized early)
StatusLed statusLed(LED_PIN);

// Zigbee devices
ZigbeeContactSensor zbContact(ENDPOINT_REED_RELAY);

bool current_contact;

void initializeZigbee() {
  zbContact.setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
  zbContact.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100, 37);
  Zigbee.addEndpoint(&zbContact);

  // Start Zigbee
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();

  if (!Zigbee.begin(&zigbeeConfig, false)) {
    DEBUG_PRINTLN("Zigbee failed to start! Rebooting...");
    DEBUG_END();
    ESP.restart();
  }

  bool is_first_pairing = esp_zb_bdb_is_factory_new();
  if (is_first_pairing) statusLed.blink(2000, 0.5); // Blink when first connecting

  DEBUG_PRINT("Connecting to Zigbee network: ");
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
    statusLed.blink(500, 0.5, 5);
    delay(5000);
  } else {
    DEBUG_PRINTLN("reconnected!");
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT);

  // Configure REED_PIN with RTC-level pull-up for deep sleep wake-up
  rtc_gpio_init((gpio_num_t)REED_PIN);
  rtc_gpio_set_direction((gpio_num_t)REED_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis((gpio_num_t)REED_PIN);
  rtc_gpio_pullup_en((gpio_num_t)REED_PIN);

  current_contact = (digitalRead(REED_PIN) == LOW); // LOW = closed

  DEBUG_INIT();
}

void loop() {
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  // Check wakeup reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT1:
      DEBUG_PRINTLN("Wakeup: reed switch state changed");
      statusLed.blink(200, 0.5, 1);
      initializeZigbee();
      if (current_contact) {
        DEBUG_PRINTLN("Contact was closed");
        zbContact.setClosed();
      } else {
        DEBUG_PRINTLN("Contact was opened");
        zbContact.setOpen();
      }
      delay(500);
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      DEBUG_PRINTLN("Wakeup: power-on/reset");
      statusLed.blink(200, 0.5, 2);
      // Check for factory reset button
      // or just `pio run --target erase`
      if (digitalRead(BUTTON_PIN) == LOW) {
        delay(100);
        int startTime = millis();
        while (digitalRead(BUTTON_PIN) == LOW) {
          delay(50);
          if ((millis() - startTime) > FACTORY_RESET_TIME_MS) {
            DEBUG_PRINTLN("Resetting Zigbee to factory");
            DEBUG_END();
            Zigbee.factoryReset();
            ESP.restart();
          }
        }
      }
      // User-initiated wakeup
      DEBUG_PRINTLN("Force reporting contact state");
      initializeZigbee();
      if (current_contact) {
        zbContact.setClosed();
      } else {
        zbContact.setOpen();
      }
      delay(500);
      break;
  }

  DEBUG_PRINT("Going to deep sleep... ");
  if (current_contact) {
    DEBUG_PRINTLN("will wake on HIGH (= contact opening)");
    esp_sleep_enable_ext1_wakeup(REED_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
  } else {
    DEBUG_PRINTLN("will wake on LOW (= contact closing)");
    esp_sleep_enable_ext1_wakeup(REED_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
  }
  DEBUG_END();

  statusLed.stopAndWait();
  esp_deep_sleep_start();
}
