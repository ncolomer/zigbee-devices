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

  esp_zb_uint48_t current_summation   = {.low = 0,    .high = 0};
  uint8_t         status              = 0;
  uint8_t         unit_of_measure     = ESP_ZB_ZCL_METERING_UNIT_M3_M3H_BINARY;
  uint8_t         summation_fmt       = 0x00;
  uint8_t         device_type         = ESP_ZB_ZCL_METERING_WATER_METERING;
  esp_zb_uint24_t multiplier          = {.low = 1,    .high = 0};
  esp_zb_uint24_t divisor             = {.low = 1000, .high = 0};
  uint16_t        liters_per_pulse    = 0;

  _metering_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_METERING);
  //       Attribute ID                                             Type                            Access                                                                Value
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, ESP_ZB_ZCL_ATTR_TYPE_U48,       ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &current_summation);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_STATUS_ID,                      ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &status);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_UNIT_OF_MEASURE_ID,             ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &unit_of_measure);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_SUMMATION_FORMATTING_ID,        ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &summation_fmt);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_METERING_DEVICE_TYPE_ID,        ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,   ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &device_type);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_MULTIPLIER_ID,                  ESP_ZB_ZCL_ATTR_TYPE_24BIT,     ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &multiplier);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID,                     ESP_ZB_ZCL_ATTR_TYPE_24BIT,     ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,                                     &divisor);
  ADD_ATTR(ATTR_LITERS_PER_PULSE,                                   ESP_ZB_ZCL_ATTR_TYPE_U16,       ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &liters_per_pulse);
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
  esp_zb_cluster_update_attr(_metering_cluster, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, &summation);
}

void ZigbeeWaterMeter::setDefaultLitersPerPulse(uint16_t liters_per_pulse) {
  esp_zb_cluster_update_attr(_metering_cluster, ATTR_LITERS_PER_PULSE, &liters_per_pulse);
}

bool ZigbeeWaterMeter::reportReadingLiters(uint32_t liters) {
  esp_zb_uint48_t zb_value = {.low = liters, .high = 0};

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, &zb_value, false
  );
  esp_zb_lock_release();
  if (status != ESP_ZB_ZCL_STATUS_SUCCESS) return false;

  esp_zb_zcl_report_attr_cmd_t cmd = {};
  cmd.address_mode     = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  cmd.attributeID      = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID;
  cmd.direction        = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  cmd.clusterID        = ESP_ZB_ZCL_CLUSTER_ID_METERING;
  cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  cmd.manuf_code       = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  cmd.dis_default_resp = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&cmd);
  esp_zb_lock_release();
  return ret == ESP_OK;
}


void ZigbeeWaterMeter::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  DEBUG_PRINTLN("ep %d: zbAttributeSet cluster=0x%04x attr=0x%04x", _endpoint, message->info.cluster, message->attribute.id);

  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_METERING) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U48) {
      esp_zb_uint48_t *val = (esp_zb_uint48_t *)message->attribute.data.value;
      uint32_t liters = val->low;
      DEBUG_PRINTLN("ep %d: water_volume = %u L", _endpoint, liters);
      if (_on_water_volume_changed != nullptr) {
        _on_water_volume_changed(liters);
      }
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
