/**
 * Zigbee2MQTT external converter for DIY Garage Water Tank monitor (XIAO ESP32C6).
 *
 * genAnalogInput presentValue (read-only, reported):
 * - EP1 water_level: tank level in %
 * - EP2 water_distance: sensor-to-water distance in cm
 *
 * genAnalogOutput presentValue (read/write, config):
 * - EP3 water_min_distance: distance in cm when the tank is full
 * - EP4 water_max_distance: distance in cm when the tank is empty
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
const nanToNull = (endpointId, property) => (model, msg) => {
    if (msg.endpoint.ID !== endpointId || msg.data.presentValue === undefined) return;
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
        m.deviceEndpoints({endpoints: {level: 1, distance: 2, min_distance: 3, max_distance: 4}}),
        m.numeric({
            name: 'water',
            label: 'Water level',
            endpointNames: ['level'],
            description: 'Water level in the tank',
            access: 'STATE_GET',
            cluster: 'genAnalogInput',
            attribute: 'presentValue',
            unit: '%',
            fzConvert: nanToNull(1, 'water_level'),
            reporting,
        }),
        m.numeric({
            name: 'water',
            label: 'Water distance',
            endpointNames: ['distance'],
            description: 'Distance from the sensor to the water surface',
            access: 'STATE_GET',
            cluster: 'genAnalogInput',
            attribute: 'presentValue',
            unit: 'cm',
            fzConvert: nanToNull(2, 'water_distance'),
            reporting,
        }),
        m.numeric({
            name: 'water',
            label: 'Full distance',
            endpointNames: ['min_distance'],
            description: 'Distance in cm when the tank is full (100%)',
            entityCategory: 'config',
            access: 'ALL',
            cluster: 'genAnalogOutput',
            attribute: 'presentValue',
            unit: 'cm',
            valueMin: 3,
            valueMax: 600,
            valueStep: 0.1,
            fzConvert: nanToNull(3, 'water_min_distance'),
        }),
        m.numeric({
            name: 'water',
            label: 'Empty distance',
            endpointNames: ['max_distance'],
            description: 'Distance in cm when the tank is empty (0%)',
            entityCategory: 'config',
            access: 'ALL',
            cluster: 'genAnalogOutput',
            attribute: 'presentValue',
            unit: 'cm',
            valueMin: 3,
            valueMax: 600,
            valueStep: 0.1,
            fzConvert: nanToNull(4, 'water_max_distance'),
        }),
    ],
    meta: {
        multiEndpoint: true,
    },
};
