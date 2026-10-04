/**
 * Zigbee2MQTT external converter for the DIY Garage Console (XIAO ESP32C6, 16 switch inputs).
 *
 * Endpoint N is connector JN, exposed as '1'..'16'. Per endpoint:
 * - switch_type (genOnOffSwitchCfg switchType, read/write): 'momentary' sends a Toggle on every
 *   press, 'toggle' follows the switch position (closed = ON) and sends On/Off.
 * - state (genOnOff onOff, read-only, reported on change): the switch position, toggle mode only.
 * - action: toggle_N / on_N / off_N, also usable as a binding source (Bind tab, genOnOff).
 *
 * Reporting max is 65000 s: 0xFFFF would cancel the reporting.
 */

import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const endpoints = Array.from({length: 16}, (_, i) => `${i + 1}`);

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageConsole', manufacturerName: 'DIY'}],
    model: 'GarageConsole',
    vendor: 'DIY',
    description: 'DIY Zigbee 16-channel switch input console',
    extend: [
        m.deviceEndpoints({endpoints: Object.fromEntries(endpoints.map((name, i) => [name, i + 1]))}),
        m.commandsOnOff({endpointNames: endpoints}),
        ...endpoints.map((endpointName) =>
            m.enumLookup({
                name: 'switch_type',
                endpointName,
                lookup: {toggle: 0, momentary: 1},
                cluster: 'genOnOffSwitchCfg',
                attribute: 'switchType',
                description: 'momentary: a press sends Toggle. toggle: follows the switch position and sends On/Off',
                access: 'ALL',
                entityCategory: 'config',
            }),
        ),
        ...endpoints.map((endpointName) =>
            m.binary({
                name: 'state',
                endpointName,
                valueOn: ['ON', 1],
                valueOff: ['OFF', 0],
                cluster: 'genOnOff',
                attribute: 'onOff',
                description: 'Switch position (toggle mode only): closed = ON',
                access: 'STATE_GET',
                reporting: {min: 0, max: 65000, change: 0},
            }),
        ),
    ],
};
