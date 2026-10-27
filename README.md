# Zigbee Devices

DIY Zigbee firmware projects for the [Seeed Studio XIAO ESP32C6](https://wiki.seeedstudio.com/xiao_esp32c6_zigbee_arduino/), built with PlatformIO and the Arduino 3 framework. All devices join a [Zigbee2MQTT](https://www.zigbee2mqtt.io/) network.

## Projects

| Project | Description | Latest release |
|---|---|---|
| `garage-console` | 16-input switch console (Zigbee toggle/on-off sources) | [![garage-console](https://img.shields.io/github/v/release/ncolomer/zigbee-devices?filter=garage-console-v*&label=)](https://github.com/ncolomer/zigbee-devices/releases?q=garage-console-v&expanded=true) |
| `garage-doors` | 4-relay switch controller for garage and yard doors/gates | [![garage-doors](https://img.shields.io/github/v/release/ncolomer/zigbee-devices?filter=garage-doors-v*&label=)](https://github.com/ncolomer/zigbee-devices/releases?q=garage-doors-v&expanded=true) |
| `garage-water` | Rain/grid water source selector + dual water meter | [![garage-water](https://img.shields.io/github/v/release/ncolomer/zigbee-devices?filter=garage-water-v*&label=)](https://github.com/ncolomer/zigbee-devices/releases?q=garage-water-v&expanded=true) |
| `garage-water-tank` | Ultrasonic water tank level monitor (OLED + Zigbee) | [![garage-water-tank](https://img.shields.io/github/v/release/ncolomer/zigbee-devices?filter=garage-water-tank-v*&label=)](https://github.com/ncolomer/zigbee-devices/releases?q=garage-water-tank-v&expanded=true) |
| `water-meter` | Battery-powered pulse counter for mechanical water meters | [![water-meter](https://img.shields.io/github/v/release/ncolomer/zigbee-devices?filter=water-meter-v*&label=)](https://github.com/ncolomer/zigbee-devices/releases?q=water-meter-v&expanded=true) |

## Hardware

- **Board**: Seeed Studio XIAO ESP32C6
- **Build tool**: PlatformIO
- **Platform**: [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32)

## Build

```bash
pio run -e debug    # debug build
pio run -e release  # release build
pio run --target upload  # flash firmware
```

## Release

Each firmware is released on its own, by pushing a `<project>-v<MAJOR.MINOR.PATCH>` tag:

```bash
git tag garage-doors-v0.1.2 && git push origin garage-doors-v0.1.2
```

[`release.yml`](.github/workflows/release.yml) builds the `release` env with that version and publishes a GitHub release with
the application image, the full flash image (`.factory.bin`), the Zigbee2MQTT external converter and, for OTA-enabled firmwares,
the Zigbee OTA image (`.ota`).
Local builds are versioned `0.0.0` unless `FIRMWARE_VERSION` is set.

Each release also refreshes the rolling `ota-index` release: its `index.json` lists the latest `.ota` of every firmware, for
Zigbee2MQTT's `ota.zigbee_ota_override_index_location`.

Firmwares opt into OTA with `custom_ota_image_type` in `platformio.ini` (unique per firmware, see [`scripts/firmware_version.py`](scripts/firmware_version.py)).

## Structure

Each project follows this layout:

```
project/
├── platformio.ini
├── src/main.cpp
└── z2m-external-converter/   # Zigbee2MQTT external converter (if needed)
```

## Libraries

Shared code lives in `libraries/` and is referenced by projects via `lib_extra_dirs`.

## Serial Monitor

```zsh
espmon() { local p=$1 b=${2:-115200}; while :; do [[ -e $p ]] && { stty -f "$p" "$b" raw && cat "$p"; } || sleep .1; done }
espmon /dev/cu.usbmodem1101
```

Reconnects automatically when the device is unplugged/replugged.
