# Garage Water Tank

Mains-powered Zigbee water tank level monitor. An A02YYUW ultrasonic sensor measures the distance to the water surface; the level is shown on a 0.91" OLED and reported over Zigbee. Port of [WaterTankMonitor](https://github.com/ncolomer/WaterTankMonitor) to the XIAO ESP32C6 (the original needed an external CC2530 for Zigbee). Board design in [`../hardware`](../hardware).

## Hardware

| Function | Pin | Notes |
|----------|-----|-------|
| Button | D0 (GPIO0) | Internal pull-up |
| OLED SDA / SCL | D4 / D5 (GPIO22 / 23) | SSD1306 128x32, I2C 0x3C |
| Sensor RX / TX | D7 / D6 (GPIO17 / 16) | A02YYUW, 9600 baud, TX idles high (filtered output mode) |
| Activity LED | LED_BUILTIN | See [Button](#button); also 1 blink on config writes, slow blink while pairing, 5 fast blinks on join |

## Behavior

- **Level** = `(max - distance) / (max - min) * 100`, clamped to 0-100. `min` is the distance when the tank is full, `max` when it is empty.
- **Distance** is the median of the last 7 sensor frames, so short echo bursts are dropped. Frames more than 10 cm off the median are discarded before entering it, unless they persist for 10 s (the level really moved). Nothing is reported until 3 frames are in, after boot or a sensor timeout.
- **Pairing**: until the device has joined, a small wifi icon blinks (1 s) on top of the meter view; it starts hidden, so a quick join never shows it. The icon also shows, steady, while a network reset is armed.
- **Display** turns on at boot and on button press, then off after 30 s. It shows the level as a bar and `%`, or `?` while the level is unknown (distances not configured, or sensor silent for 10 s).
- **High-water mark**: a 2 px line above the bar at the highest level seen (kept in NVS). It disappears when the level reaches it.
- **Zigbee** (end device): level and distance are reported on a 1 % / 1 cm change, at most every 60 s and at least every hour.

## Button

| Gesture | Effect |
|---------|--------|
| Press | 1 LED blink, display on, immediate level and distance report (acts on press, not release) |
| Double click (< 400 ms) | 2 LED blinks, high-water mark reset to the current level |
| Hold 5 s | 3 LED blinks and the wifi icon: a network reset is armed |
| Release after that | Leaves the Zigbee network and reboots into pairing mode; distances and mark are kept |

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
| `calibration_full` | Config: distance when full |
| `calibration_empty` | Config: distance when empty |

Set both distances after pairing: the level is unknown until then. They are kept in NVS.

## Build

```bash
pio run -e debug              # serial logging on USB CDC
pio run -e release
pio run -e debug -t upload
```

## Troubleshooting

- **Display shows `?`**: distances not set, or no valid sensor frame for 10 s (check the J2 wiring).
- **Re-pair**: hold the button until the wifi icon shows (5 s), then release.
- OTA is not supported yet.
