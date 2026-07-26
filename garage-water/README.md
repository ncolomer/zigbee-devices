# Garage Water

Mains-powered Zigbee **router** for a garage water console. Drop-in replacement for a
ptvo.info `garage.water` firmware, reimplemented on a Seeed XIAO ESP32-C6 with a Grove
Shield. It selects the water source with a relay and totals two water meters.

## Hardware

- **Board**: Seeed Studio XIAO ESP32C6 + Grove Shield for XIAO
- **Power**: mains via USB-C (no battery) — always awake, no deep sleep
- **Relay**: 3.3V-piloted, Normally Open, **energized on LOW**

### Pin Connections (Grove ports, first snappable half)

| Function            | Pin  | GPIO   | Notes                                              |
|---------------------|------|--------|----------------------------------------------------|
| Relay control       | D5   | 23     | ON/tank = HIGH (de-energized), OFF/grid = LOW      |
| Water meter 1 pulse | D0   | 0      | internal pull-up, FALLING interrupt, 10 ms debounce|
| Water meter 2 pulse | D1   | 1      | internal pull-up, FALLING interrupt, 10 ms debounce|
| Factory reset       | BOOT | 9      | onboard button, hold 3 s                           |
| Status LED          | —    | 15     | pairing / activity feedback                        |
| Spare               | D7   | 17     | unused                                             |

**Water source / fail-safe:** the relay idles HIGH (de-energized) via pull-up, and the
tank is wired to the de-energized side, so an **unpowered device selects the rain tank**.
Selecting grid (`OFF`) energizes the coil. Relay state is remembered across power loss.

## Behavior

- **Relay (EP1)**: Z2M on/off toggles the water source. `ON` = rain tank, `OFF` = grid.
  State persists to NVS and is restored (driven to the GPIO) on boot; a fresh device
  defaults to tank.
- **Water meters (EP2, EP3)**: each pulse adds `liters_per_pulse` to that meter's
  cumulative volume and reports it over the SE Metering cluster (`currentSummDelivered`).
- **Factory reset**: hold BOOT ≥ 3 s to leave the Zigbee network and re-pair. Meter
  totals and relay state (NVS) are preserved; a full wipe is `pio run --target erase`.

## Configuration

All configuration is done over Zigbee2MQTT after pairing:

| Entity                     | Description                                | Default |
|----------------------------|--------------------------------------------|---------|
| `state_relay`              | Water source (ON = tank, OFF = grid)       | tank    |
| `water_volume_meter1/2`    | Cumulative volume (m³, writable to calibrate) | 0    |
| `liters_per_pulse_meter1/2`| Liters per pulse (1–1000)                  | 10      |

## Storage (NVS)

- Relay state saved on every change.
- Each meter reading saved every 1000 L (1 m³) change, and immediately on a remote
  calibration write.

## Zigbee2MQTT

External converter in `z2m-external-converter/garage-water.mjs`:

1. Copy to `data/external_converters/`
2. Add to `configuration.yaml`:
   ```yaml
   external_converters:
     - garage-water.mjs
   ```
3. Restart Zigbee2MQTT

## Router mode

This firmware runs as a Zigbee **router** (`ZIGBEE_MODE_ZCZR`, `zigbee_zczr.csv`
partitions). Being permanently powered, it repeats the mesh and can parent other end
devices.

## OTA

Not implemented yet. The `zigbee_zczr.csv` partition scheme already provides the dual
OTA app slots, so OTA can be added later without repartitioning.

## Libraries

- `ZigbeeRelaySwitch` — genOnOff relay endpoint (toggle mode, NVS-persisted state)
- `ZigbeeWaterMeter` — SE Metering endpoint with custom `liters_per_pulse` attribute
- `StatusLed` — async LED blink via FreeRTOS task
- `Debug` — conditional serial logging (debug builds only)
