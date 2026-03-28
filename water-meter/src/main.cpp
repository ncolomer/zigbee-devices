#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode is not selected"
#endif

#include "Zigbee.h"
#include "ZigbeeWaterMeter.h"
#include "StatusLed.h"
#include "Debug.h"
#include <Preferences.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

/* Zigbee configuration */
#define ZIGBEE_MANUFACTURER     "DIY"
#define ZIGBEE_MODEL            "WaterMeter"

#define ENDPOINT_WATER_METER_READING      1

/* GPIO definitions */
#define LED_PIN                  LED_BUILTIN          // Built-in LED
#define BUTTON_PIN               0                    // GPIO0 / D0 / LP_GPIO0
#define PULSE_PIN                1                    // GPIO1 / D1 / A1 / LP_GPIO1
#define BUTTON_PIN_BITMASK       (1ULL << BUTTON_PIN)
#define PULSE_PIN_BITMASK        (1ULL << PULSE_PIN)
#define WAKEUP_PIN_BITMASK       (BUTTON_PIN_BITMASK | PULSE_PIN_BITMASK)
#define RTC_MAGIC_VALUE          0xDEADBEEF           // Magic value to check if RTC_DATA_ATTR was cleared

/* Configuration */
#define DEFAULT_LITERS_PER_PULSE           10         // Default 10 liters per pulse
#define SAVE_READING_THRESHOLD_L           1000u      // Save to nvs when reading changes by more than this (liters = 1 m³)

// Status LED (initialized early)
StatusLed statusLed(LED_PIN);

Preferences nvs;

RTC_DATA_ATTR uint32_t  rtc_magic              = 0;
RTC_DATA_ATTR uint32_t  rtc_liters             = 0;
RTC_DATA_ATTR uint32_t  nvs_liters             = 0;
RTC_DATA_ATTR uint16_t  rtc_liters_per_pulse   = 0;
RTC_DATA_ATTR bool      rtc_reed_closed    = false;

ZigbeeWaterMeter zbWaterMeter(ENDPOINT_WATER_METER_READING);
SemaphoreHandle_t reportSem;

void onReportResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status) {
  if (command == ZB_CMD_REPORT_ATTRIBUTE && status == ESP_ZB_ZCL_STATUS_SUCCESS) {
    xSemaphoreGive(reportSem);
  }
}

void restoreState() {
  if (rtc_magic != RTC_MAGIC_VALUE) {
    nvs.begin("water-meter", false);
    if (!nvs.isKey("reading_l")) nvs.putUInt("reading_l", 0);
    nvs_liters = nvs.getUInt("reading_l");
    rtc_liters = nvs_liters;
    if (!nvs.isKey("l_per_pulse")) nvs.putUShort("l_per_pulse", DEFAULT_LITERS_PER_PULSE);
    rtc_liters_per_pulse = nvs.getUShort("l_per_pulse");
    nvs.end();
    rtc_magic = RTC_MAGIC_VALUE;
    DEBUG_PRINTLN("Poweron: restored from NVS (reading=%u L, l/pulse=%u)", nvs_liters, rtc_liters_per_pulse);
  }
  DEBUG_PRINTLN("State: reading=%u L (nvs=%u L), l/pulse=%u", rtc_liters, nvs_liters, rtc_liters_per_pulse);
}

void saveReading(bool force = false) {
  if (force || rtc_liters - nvs_liters >= SAVE_READING_THRESHOLD_L) {
    nvs.begin("water-meter", false);
    nvs.putUInt("reading_l", rtc_liters);
    nvs.end();
    nvs_liters = rtc_liters;
    DEBUG_PRINTLN("Saved reading: %u L", rtc_liters);
  }
}

void connectZigbee() {
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  //zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;
  //Zigbee.setRxOnWhenIdle(false);
  //Zigbee.setTimeout(10000);

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
    delay(2000);
  } else {
    DEBUG_PRINTLN("reconnected!");
  }
}

void setup() {
  rtc_gpio_init((gpio_num_t)PULSE_PIN);
  rtc_gpio_set_direction((gpio_num_t)PULSE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis((gpio_num_t)PULSE_PIN);
  rtc_gpio_pullup_en((gpio_num_t)PULSE_PIN);

  rtc_gpio_init((gpio_num_t)BUTTON_PIN);
  rtc_gpio_set_direction((gpio_num_t)BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);

  DEBUG_INIT();
  DEBUG_PRINTLN("=== %s %s ===", ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);

  restoreState();

  zbWaterMeter.setManufacturerAndModel(ZIGBEE_MANUFACTURER, ZIGBEE_MODEL);
  zbWaterMeter.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100, 37);
  zbWaterMeter.setDefaultLitersPerPulse(rtc_liters_per_pulse);
  zbWaterMeter.setDefaultReadingLiters(rtc_liters);
  zbWaterMeter.onWaterVolumeChanged([](uint32_t liters) {
    liters = min(liters, (uint32_t)99999999u);
    rtc_liters = liters;
    nvs_liters = liters;
    nvs.begin("water-meter", false);
    nvs.putUInt("reading_l", liters);
    nvs.end();
    DEBUG_PRINTLN("User set: Water volume set to %u L (%.3f m³)", liters, liters / 1000.0f);
  });
  zbWaterMeter.onLitersPerPulseChanged([](uint16_t value) {
    value = constrain(value, 1, 1000);
    nvs.begin("water-meter", false);
    nvs.putUShort("l_per_pulse", value);
    nvs.end();
    rtc_liters_per_pulse = value;
    DEBUG_PRINTLN("User preset: Liters per pulse preset to %u", value);
  });
  reportSem = xSemaphoreCreateBinary();
  zbWaterMeter.onDefaultResponse(onReportResponse);
  Zigbee.addEndpoint(&zbWaterMeter);
}

void loop() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT1:
      switch (esp_sleep_get_ext1_wakeup_status()) {
        case BUTTON_PIN_BITMASK:
          DEBUG_PRINT("Wakeup: button pressed");
          while (digitalRead(BUTTON_PIN) == LOW) {
            if (millis() >= 5000) {
              DEBUG_PRINTLN(" (factory reset)");
              saveReading(true);
              connectZigbee();
              Zigbee.factoryReset(/*restart=*/true);
            }
            delay(50);
          }
          DEBUG_PRINTLN("");
          statusLed.blink(200, 0.5, 1);
          connectZigbee();
          // todo: check for OTA updates
          delay(10000); // todo: still listen to interrupts
          break;
        case PULSE_PIN_BITMASK:
          if (rtc_reed_closed) {
            DEBUG_PRINTLN("Wakeup: reed released");
            rtc_reed_closed = false;
            break;
          }
          DEBUG_PRINTLN("Wakeup: pulse detected");
          rtc_liters += rtc_liters_per_pulse;
          rtc_reed_closed = true;
          connectZigbee();
          saveReading();
          zbWaterMeter.reportReadingLiters(rtc_liters);
          DEBUG_PRINTLN("Reported reading: %u L (%.3f m³)", rtc_liters, rtc_liters / 1000.0f);
          if (xSemaphoreTake(reportSem, pdMS_TO_TICKS(5000)) == pdTRUE) {
            DEBUG_PRINTLN("Report confirmed");
          } else {
            DEBUG_PRINTLN("Report timeout (no confirmation)");
          }
          break;
      }
      break;
    default:
      DEBUG_PRINTLN("Wakeup: power-on or reset");
      connectZigbee();
      break;
  }

  DEBUG_PRINTLN("Going to deep sleep after %lums...", millis());
  DEBUG_END();

  statusLed.stopAndWait();

  // If we just counted a pulse but the reed already opened, skip the release wake
  if (rtc_reed_closed && digitalRead(PULSE_PIN) == HIGH) {
    rtc_reed_closed = false;
  }
  if (rtc_reed_closed) {
    esp_sleep_enable_ext1_wakeup(PULSE_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
  } else {
    esp_sleep_enable_ext1_wakeup(WAKEUP_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
  }
  esp_deep_sleep_start();
}
