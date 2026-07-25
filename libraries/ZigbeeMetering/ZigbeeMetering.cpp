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

#include "ZigbeeMetering.h"
#if CONFIG_ZB_ENABLED

#include "esp_zigbee_cluster.h"
#include "esp_zigbee_type.h"

esp_zb_cluster_list_t *zigbee_metering_clusters_create(zigbee_metering_cfg_t *metering) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = metering ? &(metering->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = metering ? &(metering->identify_cfg) : NULL;
  esp_zb_metering_cluster_cfg_t *metering_cfg = metering ? &(metering->metering_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_metering_cluster(cluster_list, esp_zb_metering_cluster_create(metering_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeMetering::ZigbeeMetering(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  zigbee_metering_cfg_t metering_cfg = ZIGBEE_DEFAULT_METERING_CONFIG();
  _cluster_list = zigbee_metering_clusters_create(&metering_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

esp_zb_uint48_t ZigbeeMetering::float_to_uint48(float value_m3) {
  esp_zb_uint48_t result;
  // Convert to uint64_t first, then split into low/high
  // For water meters, we typically use integer m³ or scaled values
  // Using 0.001 m³ (1 liter) resolution: multiply by 1000
  uint64_t value_scaled = (uint64_t)(value_m3 * 1000.0f);
  result.low = (uint32_t)(value_scaled & 0xFFFFFFFF);
  result.high = (uint16_t)((value_scaled >> 32) & 0xFFFF);
  return result;
}

bool ZigbeeMetering::setDefaultValue(float defaultValue_m3) {
  esp_zb_uint48_t zb_default_value = float_to_uint48(defaultValue_m3);
  esp_zb_attribute_list_t *metering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(metering_cluster, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, (void *)&zb_default_value);
  if (ret != ESP_OK) {
    log_e("Failed to set default value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMetering::setSummationDelivered(float value_m3) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  esp_zb_uint48_t zb_value = float_to_uint48(value_m3);
  log_v("Updating metering summation value...");
  log_d("Setting summation to %.3f m³ (low=0x%08X, high=0x%04X)", value_m3, zb_value.low, zb_value.high);

  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, &zb_value, false
  );
  esp_zb_lock_release();

  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set summation value: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMetering::setReporting(uint16_t min_interval, uint16_t max_interval, float delta_m3) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_METERING;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  
  // Convert delta to uint48_t for reporting
  esp_zb_uint48_t delta_uint48 = float_to_uint48(delta_m3);
  // For reporting delta, we use the low 32 bits (typical delta is small)
  reporting_info.u.send_info.delta.u48 = delta_uint48;
  
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();

  if (ret != ESP_OK) {
    log_e("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMetering::setMultiplierDivisor(esp_zb_uint24_t multiplier, esp_zb_uint24_t divisor) {
  esp_zb_attribute_list_t *metering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  
  esp_err_t ret = esp_zb_cluster_update_attr(metering_cluster, ESP_ZB_ZCL_ATTR_METERING_MULTIPLIER_ID, (void *)&multiplier);
  if (ret != ESP_OK) {
    log_e("Failed to set multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  
  ret = esp_zb_cluster_update_attr(metering_cluster, ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID, (void *)&divisor);
  if (ret != ESP_OK) {
    log_e("Failed to set divisor: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMetering::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_METERING;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();

  if (ret != ESP_OK) {
    log_e("Failed to send metering report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Metering report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
