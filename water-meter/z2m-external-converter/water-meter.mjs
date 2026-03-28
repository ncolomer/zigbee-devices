/**
 * Zigbee2MQTT external converter for DIY Zigbee Water Meter (XIAO ESP32C6).
 *
 * Endpoint 1, seMetering:
 * - water_volume: read/write currentSummDelivered (m³)
 * - liters_per_pulse: read/write 0xF001 (L), config
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const ATTR_LITERS_PER_PULSE = 0xf001;
const SE_METERING_CLUSTER_ID = 0x0702;
const ZCL_REPORT_ATTRIBUTES_CMD = 0x0a;

// Deep-sleeping ESP32 devices reset their ZCL TSN to 0 on every wake.
// zigbee-herdsman deduplicates default responses by TSN, so it may skip
// responses for repeated TSN values. Workaround: always send an explicit
// default response from a fromZigbee converter on every attributeReport.
const forceDefaultResponse = {
    fromZigbee: [{
        cluster: 'seMetering',
        type: ['attributeReport'],
        convert: (model, msg, publish, options, meta) => {
            msg.endpoint
                .defaultResponse(ZCL_REPORT_ATTRIBUTES_CMD, 0, SE_METERING_CLUSTER_ID, msg.meta.zclTransactionSequenceNumber)
                .catch((e) => console.error('defaultResponse failed:', e));
        },
    }],
    isModernExtend: true,
};

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: "WaterMeter", manufacturerName: "DIY"}],
    model: 'WaterMeter',
    vendor: 'DIY',
    description: 'DIY Zigbee water meter with pulse detection',
    extend: [
        forceDefaultResponse,
        m.battery({ percentage: false, voltage: false, lowStatus: false, percentageReporting: false }),
        m.numeric({
            name: 'water_volume',
            description: 'Cumulative water volume in m³',
            access: 'ALL',
            cluster: 'seMetering',
            attribute: 'currentSummDelivered',
            reporting: { min: 0, max: 0xFFFF, change: 0 },
            unit: 'm³',
            scale: 1000,
            precision: 3,
            valueMin: 0,
            valueMax: 99999.999,
            valueStep: 0.001,
        }),
        m.numeric({
            entityCategory: 'config',
            name: 'liters_per_pulse',
            description: 'Calibrate liters increment per pulse',
            access: 'ALL',
            cluster: 'seMetering',
            attribute: { ID: ATTR_LITERS_PER_PULSE, type: Zcl.DataType.UINT16 },
            reporting: { min: 0, max: 0xFFFF, change: 0 },
            unit: 'L',
            valueMin: 1,
            valueMax: 1000,
            valueStep: 1,
        }),
    ],
};
