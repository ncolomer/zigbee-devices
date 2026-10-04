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
 */

import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

// The level can stay flat for hours, so a 1 h max keeps the history alive
const reporting = {min: 60, max: '1_HOUR', change: 1};

// The firmware reports NaN for "unknown". The default numeric converter throws on NaN
// (assertNumber), which makes Z2M drop the message and leave the last value in place.
// Mapping it to null makes Home Assistant show the entity as unknown instead.
const nanToNull = (properties) => (model, msg) => {
    const property = properties[msg.endpoint.ID];
    if (!property || msg.data.presentValue === undefined) return;
    const value = msg.data.presentValue;
    return {[property]: Number.isNaN(value) ? null : Math.round(value * 10) / 10};
};

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageWaterTank', manufacturerName: 'DIY'}],
    model: 'GarageWaterTank',
    vendor: 'DIY',
    description: 'DIY Zigbee ultrasonic water tank level monitor',
    extend: [
        m.deviceEndpoints({endpoints: {level: 1, distance: 2, full: 3, empty: 4}}),
        // One numeric() per cluster with its own key: Z2M uses the first converter
        // matching a key, so sharing one between read-only and writable entities
        // makes every set (and get) land on the wrong one.
        m.numeric({
            name: 'water',
            endpointNames: ['level', 'distance'],
            label: ['Water level', 'Water distance'],
            description: ['Water level in the tank', 'Distance from the sensor to the water surface'],
            unit: ['%', 'cm'],
            access: 'STATE_GET',
            cluster: 'genAnalogInput',
            attribute: 'presentValue',
            fzConvert: nanToNull({1: 'water_level', 2: 'water_distance'}),
            reporting,
        }),
        m.numeric({
            name: 'calibration',
            endpointNames: ['full', 'empty'],
            label: ['Full distance', 'Empty distance'],
            description: ['Distance in cm when the tank is full (100%)', 'Distance in cm when the tank is empty (0%)'],
            unit: 'cm',
            entityCategory: 'config',
            access: 'ALL',
            cluster: 'genAnalogOutput',
            attribute: 'presentValue',
            valueMin: 3,
            valueMax: 600,
            valueStep: 0.1,
            fzConvert: nanToNull({3: 'calibration_full', 4: 'calibration_empty'}),
        }),
    ],
    meta: {
        multiEndpoint: true,
    },
};
