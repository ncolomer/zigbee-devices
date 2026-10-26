/**
 * Zigbee2MQTT external converter for DIY Garage Doors controller (XIAO ESP32C6).
 *
 * Endpoints 1-4, genOnOff:
 * - state: activates the relay (pulse or on/off depending on switch_type)
 *
 * Per-channel config, manufacturer-specific cluster 0xFC01 'relayConfig' on each endpoint (ZCL §2.6.1.3):
 * - 0x0000 switch_type: toggle (0) or momentary (1)
 * - 0x0001 pulse_duration_ms: pulse length in ms (momentary only)
 * - 0x0002 switch_actions: active_high (0) or active_low (1)
 * The standard onTime/switchType/switchActions mean something else per spec, and switchType is read-only.
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

/** @type {import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend} */
export default {
    fingerprint: [{modelID: 'GarageDoors', manufacturerName: 'DIY'}],
    model: 'GarageDoors',
    vendor: 'DIY',
    description: 'DIY Zigbee 4-channel garage door controller',
    extend: [
        m.deviceEndpoints({endpoints: {l1: 1, l2: 2, l3: 3, l4: 4}}),
        m.deviceAddCustomCluster('relayConfig', {
            name: 'relayConfig',
            ID: 0xfc01,
            attributes: {
                switchType: {name: 'switchType', ID: 0x0000, type: Zcl.DataType.ENUM8, write: true, max: 0x01},
                pulseDurationMs: {name: 'pulseDurationMs', ID: 0x0001, type: Zcl.DataType.UINT16, write: true, max: 0xffff},
                switchActions: {name: 'switchActions', ID: 0x0002, type: Zcl.DataType.ENUM8, write: true, max: 0x01},
            },
            commands: {},
            commandsResponse: {},
        }),
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
            cluster: 'relayConfig',
            attribute: 'pulseDurationMs',
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
                cluster: 'relayConfig',
                attribute: 'switchType',
            }),
            m.enumLookup({
                endpointName: ep,
                entityCategory: 'config',
                name: 'switch_actions',
                description: 'Relay activation polarity',
                lookup: {active_high: 0, active_low: 1},
                cluster: 'relayConfig',
                attribute: 'switchActions',
            }),
        ]),
    ],
    meta: {
        multiEndpoint: true,
    },
};
