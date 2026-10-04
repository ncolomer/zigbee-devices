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
const nanToNull = (endpointId, property) => (model, msg) => {
    if (msg.endpoint.ID !== endpointId || msg.data.presentValue === undefined) return;
    const value = msg.data.presentValue;
    return {[property]: Number.isNaN(value) ? null : Math.round(value * 10) / 10};
};

// Plain strings, one numeric() per endpoint: older Z2M versions don't take per-endpoint arrays
// (they print them concatenated). Entities sharing a cluster and attribute can share a key,
// but the inputs ('water') and outputs ('calibration') need separate keys: Z2M uses the first
// converter matching a key, so one shared key lands every set/get on the wrong cluster.
const input = (endpoint, id, label, description, unit) =>
    m.numeric({
        name: 'water',
        endpointNames: [endpoint],
        label,
        description,
        unit,
        access: 'STATE_GET',
        cluster: 'genAnalogInput',
        attribute: 'presentValue',
        fzConvert: nanToNull(id, `water_${endpoint}`),
        reporting,
    });

const calibration = (endpoint, id, label, description) =>
    m.numeric({
        name: 'calibration',
        endpointNames: [endpoint],
        label,
        description,
        unit: 'cm',
        entityCategory: 'config',
        access: 'ALL',
        cluster: 'genAnalogOutput',
        attribute: 'presentValue',
        valueMin: 3,
        valueMax: 600,
        valueStep: 0.1,
        fzConvert: nanToNull(id, `calibration_${endpoint}`),
    });

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageWaterTank', manufacturerName: 'DIY'}],
    model: 'GarageWaterTank',
    vendor: 'DIY',
    description: 'DIY Zigbee ultrasonic water tank level monitor',
    extend: [
        m.deviceEndpoints({endpoints: {level: 1, distance: 2, full: 3, empty: 4}}),
        input('level', 1, 'Water level', 'Water level in the tank', '%'),
        input('distance', 2, 'Water distance', 'Distance from the sensor to the water surface', 'cm'),
        calibration('full', 3, 'Full distance', 'Distance in cm when the tank is full (100%)'),
        calibration('empty', 4, 'Empty distance', 'Distance in cm when the tank is empty (0%)'),
    ],
    meta: {
        multiEndpoint: true,
    },
};
