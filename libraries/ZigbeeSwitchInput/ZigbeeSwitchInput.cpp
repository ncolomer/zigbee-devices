#include "ZigbeeSwitchInput.h"
#if CONFIG_ZB_ENABLED

#include <Arduino.h>
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "Debug.h"

// genOnOffSwitchCfg attribute ID (ZCL spec, not defined as a constant in the ESP SDK)
#define ZB_ATTR_SWITCH_CFG_SWITCH_TYPE    0x0000
#define ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS 0x0010
#define ZB_ATTR_ON_OFF_MODE               ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME  // writable mode, see the header

esp_zb_cluster_list_t *ZigbeeSwitchInput::_createClusters() {
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list,    esp_zb_basic_cluster_create(NULL),    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Server: the switch position, reported on change
  bool on_off = false;
  esp_zb_attribute_list_t *on_off_server = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
  esp_zb_cluster_add_attr(on_off_server, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_BOOL, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &on_off);
  uint16_t mode = _switch_type;
  esp_zb_cluster_add_attr(on_off_server, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZB_ATTR_ON_OFF_MODE,
                          ESP_ZB_ZCL_ATTR_TYPE_U16, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &mode);
  esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_server, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Client: lets Z2M bind this endpoint to the devices it controls
  esp_zb_cluster_list_add_on_off_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF),
                                         ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

  uint8_t switch_actions = 1;  // active low, fixed by the hardware pull-up
  _switch_cfg_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG);
  esp_zb_cluster_add_attr(_switch_cfg_cluster, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, ZB_ATTR_SWITCH_CFG_SWITCH_TYPE,
                          ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &_switch_type);
  esp_zb_cluster_add_attr(_switch_cfg_cluster, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS,
                          ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &switch_actions);
  esp_zb_cluster_list_add_on_off_switch_config_cluster(cluster_list, _switch_cfg_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return cluster_list;
}

ZigbeeSwitchInput::ZigbeeSwitchInput(uint8_t endpoint)
  : ZigbeeEP(endpoint), _switch_type(SWITCH_MOMENTARY), _state_overridden(false), _switch_type_changed(false) {
  _device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID;
  _cluster_list = _createClusters();
  _ep_config = {
    .endpoint       = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id  = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0
  };
}

void ZigbeeSwitchInput::setDefaultSwitchType(uint8_t switchType) {
  _switch_type = switchType;
  esp_zb_cluster_update_attr(_switch_cfg_cluster, ZB_ATTR_SWITCH_CFG_SWITCH_TYPE, &switchType);
  esp_zb_attribute_list_t *on_off = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  uint16_t mode = switchType;
  esp_zb_cluster_update_attr(on_off, ZB_ATTR_ON_OFF_MODE, &mode);
}

void ZigbeeSwitchInput::syncSwitchTypeMirror() {
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                               ZB_ATTR_SWITCH_CFG_SWITCH_TYPE, &_switch_type, false);
  esp_zb_lock_release();
}

bool ZigbeeSwitchInput::takeSwitchTypeChanged() {
  bool changed = _switch_type_changed;
  _switch_type_changed = false;
  return changed;
}

void ZigbeeSwitchInput::setState(bool state) {
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                               ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &state, false);

  esp_zb_zcl_report_attr_cmd_t report = {};
  report.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report.attributeID = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
  report.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
  report.zcl_basic_cmd.src_endpoint = _endpoint;
  esp_zb_zcl_report_attr_cmd_req(&report);
  esp_zb_lock_release();
}

bool ZigbeeSwitchInput::takeStateOverridden() {
  bool overridden = _state_overridden;
  _state_overridden = false;
  return overridden;
}

bool ZigbeeSwitchInput::_sendCommand(uint8_t commandId) {
  esp_zb_zcl_on_off_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;  // binding table
  cmd.on_off_cmd_id = commandId;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_on_off_cmd_req(&cmd);
  esp_zb_lock_release();
  return ret == ESP_OK;
}

bool ZigbeeSwitchInput::sendOn()     { return _sendCommand(ESP_ZB_ZCL_CMD_ON_OFF_ON_ID); }
bool ZigbeeSwitchInput::sendOff()    { return _sendCommand(ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID); }
bool ZigbeeSwitchInput::sendToggle() { return _sendCommand(ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID); }

void ZigbeeSwitchInput::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF
      && message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
    // Only the physical switch owns the state. Restoring here would deadlock on the Zigbee lock,
    // so the main loop does it.
    DEBUG_PRINTLN("ep %d: ignoring on/off command", _endpoint);
    _state_overridden = true;
    return;
  }

  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF
      && message->attribute.id == ZB_ATTR_ON_OFF_MODE
      && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
    _switch_type = *(uint16_t *)message->attribute.data.value;
    DEBUG_PRINTLN("ep %d: switch mode = %u", _endpoint, _switch_type);
    _switch_type_changed = true;
    if (_on_switch_type_changed != nullptr) {
      _on_switch_type_changed(_switch_type);
    }
  }
}

#endif  // CONFIG_ZB_ENABLED
