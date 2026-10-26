/**
 * Zigbee2MQTT external converter for DIY Garage Water controller (XIAO ESP32C6, router).
 *
 * Endpoint 1 (relay), genOnOff:
 * - state: OFF = rain tank, ON = grid water (latching, remembered across power loss)
 *
 * Endpoints 2 & 3 (meter1, meter2), seMetering:
 * - water_volume: cumulative volume (m³), read-only, backed by currentSummReceived.
 * - set_volume: write-only calibration → meterConfig 0x0000; firmware applies it and reports water_volume.
 * - liters_per_pulse: read/write meterConfig 0x0001 (L), config.
 * meterConfig is the manufacturer-specific cluster 0xFC03 (ZCL §2.6.1.3) on each meter endpoint.
 *
 * Reporting `max` must stay below 0xffff — zigbee-herdsman treats that value as "cancel
 * reporting" (ZCL §2.5.7.1.6), not a long interval.
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const meterConfig = m.deviceAddCustomCluster('meterConfig', {
    name: 'meterConfig',
    ID: 0xfc03,
    attributes: {
        setVolume: {name: 'setVolume', ID: 0x0000, type: Zcl.DataType.UINT32, write: true, max: 0xffffffff},
        litersPerPulse: {name: 'litersPerPulse', ID: 0x0001, type: Zcl.DataType.UINT16, write: true, max: 0xffff},
    },
    commands: {},
    commandsResponse: {},
});
const METER_ENDPOINTS = ['meter1', 'meter2'];
const REPORTING = {min: 0, max: 65000, change: 0};

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageWater', manufacturerName: 'DIY'}],
    model: 'GarageWater',
    vendor: 'DIY',
    description: 'DIY Zigbee garage water controller (rain/grid selector + dual water meter)',
    extend: [
        m.deviceEndpoints({endpoints: {relay: 1, meter1: 2, meter2: 3}}),
        meterConfig,
        m.onOff({
            endpointNames: ['relay'],
            description: 'Water source: OFF = rain tank, ON = grid',
            powerOnBehavior: false,
        }),
        m.numeric({
            endpointNames: METER_ENDPOINTS,
            name: 'water_volume',
            description: 'Cumulative water volume in m³',
            access: 'STATE_GET',
            cluster: 'seMetering',
            attribute: 'currentSummReceived',
            reporting: REPORTING,
            unit: 'm³',
            scale: 1000,
            precision: 3,
        }),
        m.numeric({
            endpointNames: METER_ENDPOINTS,
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
            endpointNames: METER_ENDPOINTS,
            entityCategory: 'config',
            name: 'liters_per_pulse',
            description: 'Calibrate liters increment per pulse',
            access: 'ALL',
            cluster: 'meterConfig',
            attribute: 'litersPerPulse',
            reporting: REPORTING,
            unit: 'L',
            valueMin: 1,
            valueMax: 1000,
            valueStep: 1,
        }),
    ],
    meta: {
        multiEndpoint: true,
    },
};
