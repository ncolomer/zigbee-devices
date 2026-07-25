// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Class of Zigbee Simple Metering Cluster endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_metering.h"

// clang-format off
#define ZIGBEE_DEFAULT_METERING_CONFIG()                               \
  {                                                                   \
    .basic_cfg =                                                      \
      {                                                               \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,   \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE, \
      },                                                              \
    .identify_cfg =                                                  \
      {                                                               \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE, \
      },                                                              \
    .metering_cfg =                                                  \
      {                                                               \
        .current_summation_delivered = { .low = 0, .high = 0 },      \
        .status = 0,                                                 \
        .uint_of_measure = ESP_ZB_ZCL_METERING_UNIT_M3_M3H_BINARY,   \
        .summation_formatting = 0x00,                                 \
        .metering_device_type = ESP_ZB_ZCL_METERING_WATER_METERING,  \
      },                                                              \
  }
// clang-format on

typedef struct zigbee_metering_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_metering_cluster_cfg_t metering_cfg;
} zigbee_metering_cfg_t;

class ZigbeeMetering : public ZigbeeEP {
public:
  ZigbeeMetering(uint8_t endpoint);
  ~ZigbeeMetering() {}

  // Set the cumulative consumption value in m³
  bool setSummationDelivered(float value_m3);

  // Set the default (initial) value for cumulative consumption in m³
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(float defaultValue_m3);

  // Set the reporting interval for cumulative consumption in seconds and delta (change in m³)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta_m3);

  // Report the cumulative consumption value
  bool report();

  // Set multiplier and divisor for scaling the raw value
  // Default: multiplier=1, divisor=1 (value in m³)
  bool setMultiplierDivisor(esp_zb_uint24_t multiplier, esp_zb_uint24_t divisor);

private:
  // Convert float m³ to esp_zb_uint48_t
  static esp_zb_uint48_t float_to_uint48(float value_m3);
};

#endif  // CONFIG_ZB_ENABLED
