/**
 * Zigbee2MQTT external converter for DIY Garage Water Tank monitor (XIAO ESP32C6).
 *
 * genAnalogInput presentValue (read-only, reported), key 'water':
 * - EP1 water_level: tank level in %
 * - EP2 water_distance: sensor-to-water distance in cm
 *
 * genAnalogOutput presentValue (read/write, config), key 'calibration':
 * - EP3 calibration_full: distance in cm when the tank is full (100%)
 * - EP4 calibration_empty: distance in cm when the tank is empty (0%)
 *
 * The level is unknown (null) until both distances are set,
 * and while the sensor is not answering.
 *
 * Notes on the numeric() calls below:
 * - The firmware reports NaN for "unknown". The default converter throws on NaN
 *   (assertNumber), which makes Z2M drop the message and keep the last value, so
 *   each fzConvert maps NaN to null (Home Assistant then shows the entity as unknown).
 * - The inputs ('water') and outputs ('calibration') need separate keys: Z2M uses the
 *   first converter matching a key, so a shared one lands every set/get on the wrong cluster.
 * - One call per endpoint with plain strings: older Z2M versions print per-endpoint
 *   arrays concatenated.
 */

import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageWaterTank', manufacturerName: 'DIY'}],
    model: 'GarageWaterTank',
    vendor: 'DIY',
    description: 'DIY Zigbee ultrasonic water tank level monitor',
    extend: [
        m.deviceEndpoints({endpoints: {level: 1, distance: 2, full: 3, empty: 4}}),
        m.numeric({
            name: 'water',
            endpointNames: ['level'],
            label: 'Water level',
            description: 'Water level in the tank',
            unit: '%',
            access: 'STATE_GET',
            cluster: 'genAnalogInput',
            attribute: 'presentValue',
            fzConvert: (model, msg) => {
                if (msg.endpoint.ID !== 1 || msg.data.presentValue === undefined) return;
                return {water_level: Number.isNaN(msg.data.presentValue) ? null : Math.round(msg.data.presentValue * 10) / 10};
            },
            // The level can stay flat for hours, so a 1 h max keeps the history alive
            reporting: {min: 60, max: '1_HOUR', change: 1},
        }),
        m.numeric({
            name: 'water',
            endpointNames: ['distance'],
            label: 'Water distance',
            description: 'Distance from the sensor to the water surface',
            unit: 'cm',
            access: 'STATE_GET',
            cluster: 'genAnalogInput',
            attribute: 'presentValue',
            fzConvert: (model, msg) => {
                if (msg.endpoint.ID !== 2 || msg.data.presentValue === undefined) return;
                return {water_distance: Number.isNaN(msg.data.presentValue) ? null : Math.round(msg.data.presentValue * 10) / 10};
            },
            reporting: {min: 60, max: '1_HOUR', change: 1},
        }),
        m.numeric({
            name: 'calibration',
            endpointNames: ['full'],
            label: 'Full distance',
            description: 'Distance in cm when the tank is full (100%)',
            unit: 'cm',
            entityCategory: 'config',
            access: 'ALL',
            cluster: 'genAnalogOutput',
            attribute: 'presentValue',
            valueMin: 3,
            valueMax: 600,
            valueStep: 0.1,
            fzConvert: (model, msg) => {
                if (msg.endpoint.ID !== 3 || msg.data.presentValue === undefined) return;
                return {calibration_full: Number.isNaN(msg.data.presentValue) ? null : Math.round(msg.data.presentValue * 10) / 10};
            },
        }),
        m.numeric({
            name: 'calibration',
            endpointNames: ['empty'],
            label: 'Empty distance',
            description: 'Distance in cm when the tank is empty (0%)',
            unit: 'cm',
            entityCategory: 'config',
            access: 'ALL',
            cluster: 'genAnalogOutput',
            attribute: 'presentValue',
            valueMin: 3,
            valueMax: 600,
            valueStep: 0.1,
            fzConvert: (model, msg) => {
                if (msg.endpoint.ID !== 4 || msg.data.presentValue === undefined) return;
                return {calibration_empty: Number.isNaN(msg.data.presentValue) ? null : Math.round(msg.data.presentValue * 10) / 10};
            },
        }),
    ],
    meta: {
        multiEndpoint: true,
    },
};
