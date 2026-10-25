/**
 * Zigbee2MQTT external converter for DIY Garage Doors controller (XIAO ESP32C6).
 *
 * Endpoints 1-4, genOnOff:
 * - trigger: write on_off = true to activate relay (pulse or toggle depending on switch_type)
 * - pulse_duration_ms: read/write via genOnOff onTime (0x4001), config per channel
 *
 * genOnOffSwitchCfg:
 * - switch_type: toggle (0) or momentary (1)
 * - switch_actions: active_high (0) or active_low (1)
 */

import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageDoors', manufacturerName: 'DIY'}],
    model: 'GarageDoors',
    vendor: 'DIY',
    description: 'DIY Zigbee 4-channel garage door controller',
    extend: [
        m.deviceEndpoints({endpoints: {l1: 1, l2: 2, l3: 3, l4: 4}}),
        m.onOff({
            endpointNames: ['l1', 'l2', 'l3', 'l4'],
            description: 'Activate relay (momentary pulse or toggle)',
            powerOnBehavior: false,
        }),
        m.numeric({
            endpointNames: ['l1', 'l2', 'l3', 'l4'],
            entityCategory: 'config',
            name: 'pulse_duration_ms',
            description: 'Relay pulse duration in milliseconds (momentary mode only)',
            access: 'ALL',
            cluster: 'genOnOff',
            attribute: 'onTime',
            unit: 'ms',
            valueMin: 10,
            valueMax: 10000,
            valueStep: 10,
        }),
        ...['l1', 'l2', 'l3', 'l4'].flatMap((ep) => [
            m.enumLookup({
                endpointName: ep,
                entityCategory: 'config',
                name: 'switch_type',
                description: 'Switch operating mode',
                lookup: {toggle: 0, momentary: 1},
                cluster: 'genOnOffSwitchCfg',
                attribute: 'switchType',
            }),
            m.enumLookup({
                endpointName: ep,
                entityCategory: 'config',
                name: 'switch_actions',
                description: 'Relay activation polarity',
                lookup: {active_high: 0, active_low: 1},
                cluster: 'genOnOffSwitchCfg',
                attribute: 'switchActions',
            }),
        ]),
    ],
    meta: {
        multiEndpoint: true,
    },
};
