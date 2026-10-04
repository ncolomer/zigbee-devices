# Garage Water Tank

Mains-powered Zigbee water tank level monitor. An A02YYUW ultrasonic sensor measures the distance to the water surface; the level is shown on a 0.91" OLED and reported over Zigbee. Port of [WaterTankMonitor](https://github.com/ncolomer/WaterTankMonitor) to the XIAO ESP32C6 (the original needed an external CC2530 for Zigbee). Board design in [`../hardware`](../hardware).

## Hardware

| Function | Pin | Notes |
|----------|-----|-------|
| Button | D0 (GPIO0) | Internal pull-up |
| OLED SDA / SCL | D4 / D5 (GPIO22 / 23) | SSD1306 128x32, I2C 0x3C |
| Sensor RX / TX | D7 / D6 (GPIO17 / 16) | A02YYUW, 9600 baud, TX idles high (filtered output mode) |
| Activity LED | LED_BUILTIN | Blinks on button press and on config writes |

## Behavior

- **Level** = `(max - distance) / (max - min) * 100`, clamped to 0-100. `min` is the distance when the tank is full, `max` when it is empty.
- **Pairing**: until the device has joined, a small wifi icon blinks (1 s) on top of the meter view; it starts hidden, so a quick join never shows it.
- **Display** turns on at boot, on button press, then off after 30 s. It shows the level as a bar and `%`, or `?` while the level is unknown (distances not configured, or sensor silent for 10 s).
- **High-water mark**: a 1 px line above the bar at the highest level seen (kept in NVS). It disappears when the level reaches it. Holding the button for 2 s resets it to the current level (fires without releasing).
- **Zigbee** (end device): level and distance are reported on a 1 % / 1 cm change, at most every 60 s and at least every hour.

## Zigbee2MQTT

External converter in `z2m-external-converter/garage-water-tank.mjs`:

1. Copy it to `data/external_converters/`
2. Add it to `configuration.yaml`:
   ```yaml
   external_converters:
     - garage-water-tank.mjs
   ```
3. Restart Zigbee2MQTT and pair the device.

| Entity | Description |
|--------|-------------|
| `water_level` | Level in % |
| `water_distance` | Sensor-to-water distance in cm |
| `water_min_distance` | Config: distance when full |
| `water_max_distance` | Config: distance when empty |

Set both distances after pairing: the level is unknown until then. They are kept in NVS.

## Build

```bash
pio run -e debug              # serial logging on USB CDC
pio run -e release
pio run -e debug -t upload
```

## Troubleshooting

- **Display shows `?`**: distances not set, or no valid sensor frame for 10 s (check the J2 wiring).
- **Re-pair**: `pio run -t erase`, then flash again (no factory-reset gesture yet).
- OTA is not supported yet.
