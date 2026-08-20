#include "ZigbeeWaterMeter.h"
#if CONFIG_ZB_ENABLED

#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_type.h"
#include "Debug.h"

esp_zb_cluster_list_t *ZigbeeWaterMeter::_createClusters() {
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list,    esp_zb_basic_cluster_create(NULL),    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // clang-format off
  #define ADD_ATTR(id, type, access, val) \
    esp_zb_cluster_add_attr(_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, (id), (type), (access), (val))

  esp_zb_uint48_t current_summation_rx = {.low = 0,    .high = 0};
  uint8_t         status              = 0;
  uint8_t         unit_of_measure     = ESP_ZB_ZCL_METERING_UNIT_M3_M3H_BINARY;
  uint8_t         summation_fmt       = 0x00;
  uint8_t         device_type         = ESP_ZB_ZCL_METERING_WATER_METERING;
  esp_zb_uint24_t multiplier          = {.low = 1,    .high = 0};
  esp_zb_uint24_t divisor             = {.low = 1000, .high = 0};
  uint16_t        liters_per_pulse    = 0;
  uint32_t        set_volume          = 0;

  _metering_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_METERING);
  //       Attribute ID                                             Type                            Access                                                                Value
  // currentSummDelivered is omitted: its write is hardcoded read-only on esp-zigbee-lib 1.6.8
  // regardless of declared access (esp-zigbee-sdk#856). currentSummReceived isn't auto-seeded
  // by the stack, so its reporting works (confirmed on-hardware; esp-zigbee-sdk#758) — used here
  // as the live/reported value — but its write is equally hardcoded read-only, so calibration
  // still goes through the custom ATTR_SET_VOLUME below.
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID,  ESP_ZB_ZCL_ATTR_TYPE_U48,       ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,  &current_summation_rx);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_STATUS_ID,                      ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &status);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_UNIT_OF_MEASURE_ID,             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &unit_of_measure);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_SUMMATION_FORMATTING_ID,        ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &summation_fmt);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_METERING_DEVICE_TYPE_ID,        ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &device_type);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_MULTIPLIER_ID,                  ESP_ZB_ZCL_ATTR_TYPE_24BIT,     ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &multiplier);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID,                     ESP_ZB_ZCL_ATTR_TYPE_24BIT,     ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &divisor);
  ADD_ATTR(ATTR_LITERS_PER_PULSE,                                   ESP_ZB_ZCL_ATTR_TYPE_U16,       ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &liters_per_pulse);
  ADD_ATTR(ATTR_SET_VOLUME,                                         ESP_ZB_ZCL_ATTR_TYPE_U32,       ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,                                    &set_volume);
  // clang-format on

  #undef ADD_ATTR

  esp_zb_cluster_list_add_metering_cluster(cluster_list, _metering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeWaterMeter::ZigbeeWaterMeter(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_METER_INTERFACE_DEVICE_ID;
  _cluster_list = _createClusters();
  _ep_config = {
    .endpoint       = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id  = ESP_ZB_HA_METER_INTERFACE_DEVICE_ID,
    .app_device_version = 0
  };
}

void ZigbeeWaterMeter::setDefaultReadingLiters(uint32_t liters) {
  esp_zb_uint48_t summation = {.low = liters, .high = 0};
  esp_zb_cluster_update_attr(_metering_cluster, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID, &summation);
}

void ZigbeeWaterMeter::setDefaultLitersPerPulse(uint16_t liters_per_pulse) {
  esp_zb_cluster_update_attr(_metering_cluster, ATTR_LITERS_PER_PULSE, &liters_per_pulse);
}

// No lock; assumes it is safe to call the Zigbee SDK directly (lock held, or running
// inside a Zigbee callback per esp_zb_lock_acquire()'s contract).
bool ZigbeeWaterMeter::_setAttr(uint32_t liters) {
  esp_zb_uint48_t zb_value = {.low = liters, .high = 0};
  return esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID, &zb_value, false
  ) == ESP_ZB_ZCL_STATUS_SUCCESS;
}

// No lock. Forces an explicit report, bypassing Zigbee2MQTT's configured reporting interval —
// used for calibration, where the user expects the new value to show up immediately.
bool ZigbeeWaterMeter::_forceReport() {
  esp_zb_zcl_report_attr_cmd_t cmd = {};
  cmd.address_mode     = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  cmd.attributeID      = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_RECEIVED_ID;
  cmd.direction        = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  cmd.clusterID        = ESP_ZB_ZCL_CLUSTER_ID_METERING;
  cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  cmd.manuf_code       = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  cmd.dis_default_resp = 0;
  return esp_zb_zcl_report_attr_cmd_req(&cmd) == ESP_OK;
}

bool ZigbeeWaterMeter::setReadingLiters(uint32_t liters, bool forceReport) {
  esp_zb_lock_acquire(portMAX_DELAY);
  bool ok = _setAttr(liters);
  if (ok && forceReport) ok = _forceReport();
  esp_zb_lock_release();
  return ok;
}


void ZigbeeWaterMeter::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  DEBUG_PRINTLN("ep %d: zbAttributeSet cluster=0x%04x attr=0x%04x", _endpoint, message->info.cluster, message->attribute.id);

  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_METERING) {
    // currentSummReceived can't be written directly (stack-enforced, like currentSummDelivered);
    // calibration comes in via this custom attribute instead.
    if (message->attribute.id == ATTR_SET_VOLUME
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U32) {
      uint32_t liters = *(uint32_t *)message->attribute.data.value;
      DEBUG_PRINTLN("ep %d: set_volume = %u L", _endpoint, liters);
      if (_on_water_volume_changed != nullptr) {
        _on_water_volume_changed(liters);
      }
      // Already in a Zigbee callback (no lock); force-report so the new value shows up now.
      if (_setAttr(liters)) _forceReport();
      return;
    }

    if (message->attribute.id == ATTR_LITERS_PER_PULSE
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t preset_value = *(uint16_t *)message->attribute.data.value;
      DEBUG_PRINTLN("ep %d: liters_per_pulse = %u", _endpoint, preset_value);
      if (_on_liters_per_pulse_changed != nullptr) {
        _on_liters_per_pulse_changed(preset_value);
      }
      return;
    }
  }
}

#endif  // CONFIG_ZB_ENABLED
