/**
 * Zigbee2MQTT external converter for DIY Zigbee Water Meter (XIAO ESP32C6).
 *
 * Endpoint 1, seMetering:
 * - water_volume: cumulative volume (m³), read-only, backed by currentSummReceived (the ZigbeeWaterMeter library)
 * Endpoint 1, meterConfig (manufacturer-specific cluster 0xFC03, ZCL §2.6.1.3):
 * - set_volume: write-only calibration → 0x0000; firmware applies it and reports water_volume
 * - liters_per_pulse: read/write 0x0001 (L), config
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

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
        m.deviceAddCustomCluster('meterConfig', {
            name: 'meterConfig',
            ID: 0xfc03,
            attributes: {
                setVolume: {name: 'setVolume', ID: 0x0000, type: Zcl.DataType.UINT32, write: true, max: 0xffffffff},
                litersPerPulse: {name: 'litersPerPulse', ID: 0x0001, type: Zcl.DataType.UINT16, write: true, max: 0xffff},
            },
            commands: {},
            commandsResponse: {},
        }),
        m.battery({ percentage: false, voltage: false, lowStatus: false, percentageReporting: false }),
        m.numeric({
            name: 'water_volume',
            description: 'Cumulative water volume in m³',
            access: 'STATE_GET',
            cluster: 'seMetering',
            attribute: 'currentSummReceived',
            reporting: { min: 0, max: 65000, change: 0 },
            unit: 'm³',
            scale: 1000,
            precision: 3,
        }),
        m.numeric({
            entityCategory: 'config',
            name: 'set_volume',
            description: 'Set/calibrate the cumulative water volume (m³)',
            access: 'SET',
            cluster: 'meterConfig',
            attribute: 'setVolume',
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
            cluster: 'meterConfig',
            attribute: 'litersPerPulse',
            reporting: { min: 0, max: 65000, change: 0 },
            unit: 'L',
            valueMin: 1,
            valueMax: 1000,
            valueStep: 1,
        }),
    ],
};
