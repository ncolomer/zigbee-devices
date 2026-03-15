#include "ZigbeeRelaySwitch.h"
#if CONFIG_ZB_ENABLED

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_type.h"
#include "Debug.h"

// genOnOffSwitchCfg attribute IDs (ZCL spec, not defined as constants in ESP SDK)
#define ZB_ATTR_SWITCH_CFG_SWITCH_TYPE    0x0000
#define ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS 0x0010

esp_zb_cluster_list_t *ZigbeeRelaySwitch::_createClusters() {
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list,    esp_zb_basic_cluster_create(NULL),    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // clang-format off
  #define ADD_ATTR(id, type, access, val) \
    esp_zb_cluster_add_attr(_on_off_cluster, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, (id), (type), (access), (val))

  bool     on_off         = false;
  uint16_t pulse_duration = _pulse_duration_ms;

  _on_off_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, ESP_ZB_ZCL_ATTR_TYPE_BOOL, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &on_off);
  ADD_ATTR(ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME,   ESP_ZB_ZCL_ATTR_TYPE_U16,  ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &pulse_duration);
  // clang-format on

  #undef ADD_ATTR

  esp_zb_cluster_list_add_on_off_cluster(cluster_list, _on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Create genOnOffSwitchCfg cluster manually to override ZCL spec's read-only on switchType
  // clang-format off
  #define ADD_ATTR(id, type, access, val) \
    esp_zb_cluster_add_attr(_switch_cfg_cluster, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, (id), (type), (access), (val))

  _switch_cfg_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG);
  ADD_ATTR(ZB_ATTR_SWITCH_CFG_SWITCH_TYPE,    ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &_switch_type);
  ADD_ATTR(ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS, ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &_switch_actions);
  // clang-format on

  #undef ADD_ATTR

  esp_zb_cluster_list_add_on_off_switch_config_cluster(cluster_list, _switch_cfg_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return cluster_list;
}

ZigbeeRelaySwitch::ZigbeeRelaySwitch(uint8_t endpoint, uint8_t gpioPin)
  : ZigbeeEP(endpoint), _gpio_pin(gpioPin), _pulse_duration_ms(250),
    _switch_type(0), _switch_actions(0), _pulsing(false), _relay_state(false) {
  _device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID;
  _cluster_list = _createClusters();
  _ep_config = {
    .endpoint       = _endpoint,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id  = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
    .app_device_version = 0
  };
}

void ZigbeeRelaySwitch::_applyIdleState() {
  digitalWrite(_gpio_pin, (_switch_actions == 0) ? LOW : HIGH);
}

void ZigbeeRelaySwitch::begin() {
  pinMode(_gpio_pin, OUTPUT);
  _applyIdleState();
}

void ZigbeeRelaySwitch::setDefaultPulseDuration(uint16_t pulseDurationMs) {
  _pulse_duration_ms = pulseDurationMs;
  esp_zb_cluster_update_attr(_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, &pulseDurationMs);
}

void ZigbeeRelaySwitch::setDefaultSwitchType(uint8_t switchType) {
  _switch_type = switchType;
  esp_zb_cluster_update_attr(_switch_cfg_cluster, ZB_ATTR_SWITCH_CFG_SWITCH_TYPE, &switchType);
}

void ZigbeeRelaySwitch::setDefaultSwitchActions(uint8_t switchActions) {
  _switch_actions = switchActions;
  esp_zb_cluster_update_attr(_switch_cfg_cluster, ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS, &switchActions);
}

bool ZigbeeRelaySwitch::setOnOff(bool state) {
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_status_t ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &state, false
  );
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

void ZigbeeRelaySwitch::_pulseTask(void *arg) {
  ZigbeeRelaySwitch *self = static_cast<ZigbeeRelaySwitch *>(arg);
  uint8_t active = (self->_switch_actions == 0) ? HIGH : LOW;
  digitalWrite(self->_gpio_pin, active);
  vTaskDelay(pdMS_TO_TICKS(self->_pulse_duration_ms));
  digitalWrite(self->_gpio_pin, !active);
  self->setOnOff(false);
  self->_pulsing = false;
  vTaskDelete(NULL);
}

void ZigbeeRelaySwitch::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  DEBUG_PRINTLN("ep %d: zbAttributeSet cluster=0x%04x attr=0x%04x", _endpoint, message->info.cluster, message->attribute.id);

  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      bool value = *(bool *)message->attribute.data.value;
      if (_switch_type == 1) {
        // momentary mode: pulse on rising edge
        if (value && !_pulsing) {
          DEBUG_PRINTLN("ep %d: pulse start (%u ms)", _endpoint, _pulse_duration_ms);
          _pulsing = true;
          xTaskCreate(_pulseTask, "pulse", 2048, this, 5, NULL);
        }
      } else {
        // toggle mode: drive GPIO directly
        _relay_state = value;
        uint8_t active_level = (_switch_actions == 0) ? HIGH : LOW;
        digitalWrite(_gpio_pin, value ? active_level : !active_level);
        DEBUG_PRINTLN("ep %d: relay %s", _endpoint, value ? "ON" : "OFF");
      }
      return;
    }

    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t ms = *(uint16_t *)message->attribute.data.value;
      DEBUG_PRINTLN("ep %d: new pulse_duration_ms = %u", _endpoint, ms);
      _pulse_duration_ms = ms;
      if (_on_pulse_duration_changed != nullptr) {
        _on_pulse_duration_changed(ms);
      }
      return;
    }
  }

  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG) {
    if (message->attribute.id == ZB_ATTR_SWITCH_CFG_SWITCH_TYPE
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
      uint8_t val = *(uint8_t *)message->attribute.data.value;
      DEBUG_PRINTLN("ep %d: switchType = %u", _endpoint, val);
      _switch_type = val;
      if (_on_switch_type_changed != nullptr) {
        _on_switch_type_changed(val);
      }
      return;
    }

    if (message->attribute.id == ZB_ATTR_SWITCH_CFG_SWITCH_ACTIONS
        && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
      uint8_t val = *(uint8_t *)message->attribute.data.value;
      DEBUG_PRINTLN("ep %d: switchActions = %u", _endpoint, val);
      _switch_actions = val;
      _applyIdleState();
      if (_on_switch_actions_changed != nullptr) {
        _on_switch_actions_changed(val);
      }
      return;
    }
  }
}

#endif  // CONFIG_ZB_ENABLED
