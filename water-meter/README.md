# Water Meter

Battery-powered Zigbee pulse counter for mechanical water meters equipped with a reed switch. Counts pulses during deep sleep via EXT1 wakeup, reports cumulative volume over Zigbee using the SE Metering cluster (`currentSummDelivered`).

## Hardware

- **Board**: Seeed Studio XIAO ESP32C6
- **Sensor**: Reed switch pulse emitter (e.g. [Maddalena Reed Switch](https://www.maddalena.it/en/product/reed-switch/))
- **Power**: Battery (connects to BAT+/BAT- pads)

### Pin Connections

| Function | GPIO | Notes |
|----------|------|-------|
| Pulse input | 1 (LP_GPIO1) | Reed switch signal, internal pull-up |
| Button | 0 (LP_GPIO0) | Hold 5s for factory reset |
| LED | LED_BUILTIN | Status feedback (pairing, button press) |

## Deep Sleep Behavior

The device spends virtually all its time in deep sleep. Wake sources:

- **Pulse (GPIO 1 falling)**: Increments reading by `liters_per_pulse`, connects to Zigbee, reports, sleeps.
- **Button (GPIO 0 falling)**: Blinks LED, connects to Zigbee, stays awake 10s (for future OTA), sleeps. Hold 5s for factory reset.
- **Power-on/reset**: Connects to Zigbee, sleeps.

Reed switch debouncing is handled via a two-phase wakeup: after a pulse (reed closes), the next wakeup on reed release is ignored.

## Configuration

All configuration is done remotely via Zigbee2MQTT after pairing:

| Entity | Description | Default |
|--------|-------------|---------|
| `water_volume` | Cumulative volume (m³, writable for calibration) | 0 |
| `liters_per_pulse` | Liters per reed pulse (1–1000) | 10 |

## Storage

- **RTC memory**: Reading persists across deep sleep cycles (lost on power loss).
- **NVS**: Reading saved to flash every 1000 L (1 m³) change, and on user calibration writes.

## Zigbee2MQTT

External converter in `z2m-external-converter/water-meter.mjs`. Install:

1. Copy to `data/external_converters/`
2. Add to `configuration.yaml`:
   ```yaml
   external_converters:
     - water-meter.mjs
   ```
3. Restart Zigbee2MQTT

## Libraries

- `ZigbeeWaterMeter` — SE Metering endpoint with custom `liters_per_pulse` attribute
- `StatusLed` — async LED blink via FreeRTOS task
- `Debug` — conditional serial logging (debug builds only)
