#include "ZigbeeContactSensor.h"

#include "ha/esp_zigbee_ha_standard.h"

ZigbeeContactSensor::ZigbeeContactSensor(uint8_t endpoint)
    : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_IAS_ZONE_ID;

  esp_zb_basic_cluster_cfg_t basic_cfg = {};
  esp_zb_identify_cluster_cfg_t identify_cfg = {};
  esp_zb_ias_zone_cluster_cfg_t ias_cfg = {
    .zone_state = ESP_ZB_ZCL_IAS_ZONE_ZONESTATE_ENROLLED,
    .zone_type = ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_CONTACT_SWITCH,
    .zone_status = 0,
    .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
    .zone_id = 0x00,
    .zone_ctx = {0},
  };

  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(&basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(&identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_ias_zone_cluster(_cluster_list, esp_zb_ias_zone_cluster_create(&ias_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {
    .endpoint = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_IAS_ZONE_ID,
    .app_device_version = 0,
  };
}

void ZigbeeContactSensor::setClosed() {
  setContactState(true);
}

void ZigbeeContactSensor::setOpen() {
  setContactState(false);
}

void ZigbeeContactSensor::setContactState(bool isClosed) {
  uint16_t zoneStatus = isClosed ? ZONE_STATUS_CLOSED : ZONE_STATUS_OPEN;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint,
    ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID,
    &zoneStatus,
    false
  );
  esp_zb_lock_release();

  esp_zb_zcl_ias_zone_status_change_notif_cmd_t notif = {};
  notif.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  notif.zcl_basic_cmd.src_endpoint = _endpoint;
  notif.zone_status = zoneStatus;
  notif.extend_status = 0;
  notif.zone_id = 0;
  notif.delay = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&notif);
  esp_zb_lock_release();
}
