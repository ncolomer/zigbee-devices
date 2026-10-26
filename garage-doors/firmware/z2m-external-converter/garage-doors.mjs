/**
 * Zigbee2MQTT external converter for DIY Garage Doors controller (XIAO ESP32C6).
 *
 * Endpoints 1-4, genOnOff:
 * - state: activates the relay (pulse or on/off depending on switch_type)
 *
 * Per-channel config, manufacturer-specific genOnOff attributes (code 0x1234, ZCL §2.6.1.4):
 * - 0x0100 switch_type: toggle (0) or momentary (1)
 * - 0x0101 pulse_duration_ms: pulse length in ms (momentary only)
 * - 0x0102 switch_actions: active_high (0) or active_low (1)
 * The standard onTime/switchType/switchActions mean something else per spec, and switchType is read-only.
 */

import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const MANUF = {manufacturerCode: 0x1234};
const ATTR_SWITCH_TYPE = {ID: 0x0100, type: Zcl.DataType.ENUM8};
const ATTR_PULSE_MS = {ID: 0x0101, type: Zcl.DataType.UINT16};
const ATTR_SWITCH_ACTIONS = {ID: 0x0102, type: Zcl.DataType.ENUM8};

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
            attribute: ATTR_PULSE_MS,
            zigbeeCommandOptions: MANUF,
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
                cluster: 'genOnOff',
                attribute: ATTR_SWITCH_TYPE,
                zigbeeCommandOptions: MANUF,
            }),
            m.enumLookup({
                endpointName: ep,
                entityCategory: 'config',
                name: 'switch_actions',
                description: 'Relay activation polarity',
                lookup: {active_high: 0, active_low: 1},
                cluster: 'genOnOff',
                attribute: ATTR_SWITCH_ACTIONS,
                zigbeeCommandOptions: MANUF,
            }),
        ]),
    ],
    meta: {
        multiEndpoint: true,
    },
};
