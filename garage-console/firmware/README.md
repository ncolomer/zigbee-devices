# Garage Console

Mains-powered Zigbee console with 16 switch inputs. Port of the CC2530/PTVO garage console to the XIAO ESP32C6, generalized to 16 identical channels. Board design in [`../hardware`](../hardware).

## Hardware

| Function | Pin | Notes |
|----------|-----|-------|
| 16 switch inputs J1..J16 | PCF8575 @ 0x20 | Closed contact = low, no external pull-up |
| I²C SDA / SCL | D4 / D5 (GPIO22 / 23) | |
| Expander INT | D2 (GPIO2) | Active low, falling edge |
| Activity LED | LED_BUILTIN | |
| Factory reset | BOOT (GPIO9) | Hold 3 s |

## Behavior

Endpoint N is connector JN. Each channel has a `switch_type`, kept in NVS (default `momentary`):

- **momentary**: closing the contact sends an On/Off **Toggle** command to the endpoint's bindings.
- **toggle**: the endpoint's `state` follows the switch (closed = ON), is reported on every change, and an **On/Off** command is sent to the bindings. The position at boot is reported without a command.

`state` is read-only: only the physical switch changes it. Nothing is reported periodically.

LED: slow blink while pairing, 5 quick blinks once joined, short blink on every command sent. Inputs are sampled on the expander interrupt plus a 1 s poll, with a 30 ms debounce.

End device with the receiver always on (mains-powered), so bindings and config writes apply immediately.

## Zigbee2MQTT

External converter in `z2m-external-converter/garage-console.mjs`:

1. Copy it to `data/external_converters/`
2. Add it to `configuration.yaml`:
   ```yaml
   external_converters:
     - garage-console.mjs
   ```
3. Restart Zigbee2MQTT and pair the device.

| Entity (per endpoint `1`..`16`) | Description |
|--------|-------------|
| `action` | `toggle_N`, `on_N`, `off_N` |
| `switch_type_N` | Config: `momentary` or `toggle` |
| `state_N` | Switch position (toggle mode) |

Bind an endpoint (Bind tab, On/off cluster) to a device to control it directly, e.g. `10` to a garage-doors relay.

## Build

```bash
pio run -e debug              # serial logging on USB CDC
pio run -e release
pio run -e debug -t upload
```

## Troubleshooting

- **No input events**: `PCF8575 not found` in the debug log means the I²C wiring or address is wrong.
- **Reconfigure in Z2M logs `TABLE_FULL`**: expected after the first successful configure. The ESP stack rejects a bind that already exists; the existing bindings are untouched.
- **Re-pair**: hold BOOT for 3 s.
- OTA is not supported yet.
