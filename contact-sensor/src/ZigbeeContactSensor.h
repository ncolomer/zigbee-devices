#pragma once

#include "Zigbee.h"
#include "ZigbeeEP.h"

/**
 * Minimal binary contact sensor endpoint, exposed as a simple sensor.
 * Keeps the old helper API so existing sketches can reuse it.
 */
class ZigbeeContactSensor : public ZigbeeEP {
public:
    explicit ZigbeeContactSensor(uint8_t endpoint);

    void setClosed();
    void setOpen();

private:
    void setContactState(bool isClosed);

    static constexpr uint16_t ZONE_STATUS_CLOSED = 0;
    static constexpr uint16_t ZONE_STATUS_OPEN = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 | ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2;
};
