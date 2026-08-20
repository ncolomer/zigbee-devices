/**
 * Zigbee2MQTT external converter for DIY Garage Water controller (XIAO ESP32C6, router).
 *
 * Endpoint 1 (relay), genOnOff:
 * - state: ON = rain tank, OFF = grid water (latching, remembered across power loss)
 *
 * Endpoints 2 & 3 (meter1, meter2), seMetering:
 * - water_volume: cumulative volume (m³), read-only. Backed by currentSummReceived, not
 *   currentSummDelivered — the latter's reporting is broken on esp-zigbee-lib 1.6.8
 *   (esp-zigbee-sdk#758); semantically repurposed, private-network-only workaround. No standard
 *   summation attribute is writable on this stack (esp-zigbee-sdk#856), so calibration goes
 *   through set_volume instead.
 * - set_volume: write-only calibration → custom 0xF000; firmware applies it and reports water_volume.
 * - liters_per_pulse: read/write 0xF001 (L), config.
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const ATTR_LITERS_PER_PULSE = 0xf001;
const ATTR_SET_VOLUME = 0xf000;
const METER_ENDPOINTS = ['meter1', 'meter2'];

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageWater', manufacturerName: 'DIY'}],
    model: 'GarageWater',
    vendor: 'DIY',
    description: 'DIY Zigbee garage water controller (rain/grid selector + dual water meter)',
    extend: [
        m.deviceEndpoints({endpoints: {relay: 1, meter1: 2, meter2: 3}}),
        m.onOff({
            endpointNames: ['relay'],
            description: 'Water source: ON = rain tank, OFF = grid',
            powerOnBehavior: false,
        }),
        m.numeric({
            endpointNames: METER_ENDPOINTS,
            name: 'water_volume',
            description: 'Cumulative water volume in m³',
            access: 'STATE_GET',
            cluster: 'seMetering',
            attribute: 'currentSummReceived',
            reporting: {min: 0, max: 0xffff, change: 0},
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
            cluster: 'seMetering',
            attribute: {ID: ATTR_SET_VOLUME, type: Zcl.DataType.UINT32},
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
            cluster: 'seMetering',
            attribute: {ID: ATTR_LITERS_PER_PULSE, type: Zcl.DataType.UINT16},
            reporting: {min: 0, max: 0xffff, change: 0},
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
