#pragma once

#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include <functional>

// Physical switch input exposed as a Zigbee endpoint.
// genOnOffSwitchCfg switchType (0x0000): 0=toggle, 1=momentary
// Commands go to the endpoint's binding table, so bindings made from Zigbee2MQTT work.

class ZigbeeSwitchInput : public ZigbeeEP {
public:
  static constexpr uint8_t SWITCH_TOGGLE = 0;
  static constexpr uint8_t SWITCH_MOMENTARY = 1;

  ZigbeeSwitchInput(uint8_t endpoint);
  ~ZigbeeSwitchInput() {}

  void setDefaultSwitchType(uint8_t switchType);
  uint8_t switchType() { return _switch_type; }

  void onSwitchTypeChanged(std::function<void(uint8_t)> callback) {
    _on_switch_type_changed = callback;
  }

  // Updates the onOff attribute and reports it
  void setState(bool state);
  // True once if a coordinator write overrode the state; call setState() again to restore it
  bool takeStateOverridden();

  bool sendOn();
  bool sendOff();
  bool sendToggle();

protected:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

private:
  uint8_t _switch_type;
  bool _state;
  volatile bool _state_overridden;

  esp_zb_attribute_list_t *_switch_cfg_cluster = nullptr;

  esp_zb_cluster_list_t *_createClusters();
  bool _sendCommand(uint8_t commandId);

  std::function<void(uint8_t)> _on_switch_type_changed = nullptr;
};

#endif  // CONFIG_ZB_ENABLED
