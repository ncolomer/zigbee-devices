#pragma once

#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include <functional>

// Settings live in a manufacturer-specific cluster on the endpoint (ZCL §2.6.1.3): non-standard
// attribute IDs in seMetering are reserved by the spec (§2.6.1.4).
#define ZB_CLUSTER_METER_CONFIG 0xFC03
// seMetering summation attributes aren't writable on esp-zigbee-lib 1.6.8; calibrate via this instead.
#define ATTR_SET_VOLUME         0x0000  // uint32, liters
#define ATTR_LITERS_PER_PULSE   0x0001  // uint16, liters

class ZigbeeWaterMeter : public ZigbeeEP {
public:
  ZigbeeWaterMeter(uint8_t endpoint);
  ~ZigbeeWaterMeter() {}

  void setDefaultReadingLiters(uint32_t liters);
  void setDefaultLitersPerPulse(uint16_t liters_per_pulse);

  void onWaterVolumeChanged(std::function<void(uint32_t)> callback) {
    _on_water_volume_changed = callback;
  }

  void onLitersPerPulseChanged(std::function<void(uint16_t)> callback) {
    _on_liters_per_pulse_changed = callback;
  }

  // forceReport bypasses the configured reporting interval to transmit immediately.
  bool setReadingLiters(uint32_t liters, bool forceReport = false);

protected:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

private:
  esp_zb_attribute_list_t *_metering_cluster = nullptr;
  esp_zb_attribute_list_t *_config_cluster = nullptr;

  esp_zb_cluster_list_t *_createClusters();
  // No lock; call with the Zigbee lock held or from a callback.
  bool _setAttr(uint32_t liters);
  bool _forceReport();

  std::function<void(uint32_t)> _on_water_volume_changed = nullptr;
  std::function<void(uint16_t)> _on_liters_per_pulse_changed = nullptr;
};

#endif  // CONFIG_ZB_ENABLED
