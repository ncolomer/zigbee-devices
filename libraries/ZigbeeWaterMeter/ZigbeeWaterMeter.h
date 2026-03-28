#pragma once

#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include <functional>

// Custom attribute IDs (manufacturer-specific)
#define ATTR_LITERS_PER_PULSE 0xF001

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

  bool reportReadingLiters(uint32_t liters);

protected:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

private:
  esp_zb_attribute_list_t *_metering_cluster = nullptr;

  esp_zb_cluster_list_t *_createClusters();

  std::function<void(uint32_t)> _on_water_volume_changed = nullptr;
  std::function<void(uint16_t)> _on_liters_per_pulse_changed = nullptr;
};

#endif  // CONFIG_ZB_ENABLED
