#pragma once

#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include <functional>

// genOnOff onTime (0x4001) reused as pulse duration storage (ms)
// genOnOffSwitchCfg switchType (0x0000): 0=toggle, 1=momentary
// genOnOffSwitchCfg switchActions (0x0010): 0=active_high, 1=active_low

class ZigbeeRelaySwitch : public ZigbeeEP {
public:
  ZigbeeRelaySwitch(uint8_t endpoint, uint8_t gpioPin);
  ~ZigbeeRelaySwitch() {}

  void begin();
  void setDefaultPulseDuration(uint16_t pulseDurationMs);
  void setDefaultSwitchType(uint8_t switchType);
  void setDefaultSwitchActions(uint8_t switchActions);

  void onPulseDurationChanged(std::function<void(uint16_t)> callback) {
    _on_pulse_duration_changed = callback;
  }
  void onSwitchTypeChanged(std::function<void(uint8_t)> callback) {
    _on_switch_type_changed = callback;
  }
  void onSwitchActionsChanged(std::function<void(uint8_t)> callback) {
    _on_switch_actions_changed = callback;
  }

  bool setOnOff(bool state);

protected:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

private:
  uint8_t _gpio_pin;
  uint16_t _pulse_duration_ms;
  uint8_t _switch_type;    // 0=toggle, 1=momentary
  uint8_t _switch_actions; // 0=active_high, 1=active_low
  volatile bool _pulsing;
  bool _relay_state; // for toggle mode

  esp_zb_attribute_list_t *_on_off_cluster = nullptr;
  esp_zb_attribute_list_t *_switch_cfg_cluster = nullptr;

  esp_zb_cluster_list_t *_createClusters();
  static void _pulseTask(void *arg);
  void _applyIdleState();

  std::function<void(uint16_t)> _on_pulse_duration_changed = nullptr;
  std::function<void(uint8_t)> _on_switch_type_changed = nullptr;
  std::function<void(uint8_t)> _on_switch_actions_changed = nullptr;
};


#endif  // CONFIG_ZB_ENABLED
