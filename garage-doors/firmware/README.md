# Garage Doors

Mains-powered Zigbee router driving 4 relays for the garage and yard doors/gates, on a XIAO ESP32C6 with a Grove Base. Each relay is one endpoint, either pulsed (to trigger a door opener) or held on/off.

## Hardware

| Function | Pin | Notes |
|----------|-----|-------|
| Relay 1..4 | D1, D2, D4, D6 | Endpoints 1..4, Grove Base headers |
| Activity LED | LED_BUILTIN | |
| Factory reset | BOOT (GPIO9) | Hold 3 s |

## Behavior

Each channel has three settings, kept in NVS and written from Zigbee2MQTT:

| Setting | Values | Default |
|---------|--------|---------|
| `switch_type` | `momentary`: an On pulses the relay for `pulse_duration_ms`, then the state goes back to Off. `toggle`: the relay follows On/Off | `toggle` |
| `switch_actions` | `active_high` or `active_low` relay input | `active_high` |
| `pulse_duration_ms` | 10..10000 ms (momentary only) | 250 |

In toggle mode the relay state is not persisted: it boots idle (Off).

Over Zigbee the three settings are attributes of a manufacturer-specific cluster `0xFC01` on each endpoint: `0x0000` switch type, `0x0001` pulse duration (ms), `0x0002` switch actions. The standard `onTime` and On/Off Switch Configuration attributes are not used: the ZCL defines them for timed-off and for switch devices, and `switchType` is read-only.

LED: slow blink while pairing, 5 quick blinks once joined.

Router (mains-powered): the receiver is always on, so commands and config writes apply immediately, and it relays for other garage devices and reroutes on its own if a neighbour drops.

## Zigbee2MQTT

External converter in `z2m-external-converter/garage-doors.mjs`:

1. Copy it to `data/external_converters/`
2. Add it to `configuration.yaml`:
   ```yaml
   external_converters:
     - garage-doors.mjs
   ```
3. Restart Zigbee2MQTT and pair the device.

| Entity (per endpoint `l1`..`l4`) | Description |
|--------|-------------|
| `state` | Activate the relay (pulse or on/off, depending on `switch_type`) |
| `switch_type` | Config: `toggle` or `momentary` |
| `switch_actions` | Config: `active_high` or `active_low` |
| `pulse_duration_ms` | Config: pulse length in ms |

The relays can also be driven directly by a bound device, e.g. a Garage Console endpoint bound to `genOnOff`.

## Build

```bash
pio run -e debug              # serial logging on USB CDC
pio run -e release
pio run -e debug -t upload
```

## OTA

Endpoint 1 carries the OTA client (manufacturer `0x1001`, image type `0xD101`). Once joined, it queries Zigbee2MQTT every hour.

Each `garage-doors-vX.Y.Z` release lands in the rolling `ota-index` release, which Zigbee2MQTT polls. Zigbee2MQTT then flags
the update; trigger it from *OTA → Update* (about 1 h with Zigbee2MQTT's default 50-byte blocks). The device reboots into the new image.

## Troubleshooting

- **Relay inverted** (energized at idle): set `switch_actions` to match the relay module's input polarity.
- **Door doesn't trigger**: use `momentary`, and raise `pulse_duration_ms` if the opener needs a longer contact.
- **Re-pair**: hold BOOT for 3 s.
