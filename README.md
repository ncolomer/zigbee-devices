# Zigbee Devices

DIY Zigbee firmware projects for the [Seeed Studio XIAO ESP32C6](https://wiki.seeedstudio.com/xiao_esp32c6_zigbee_arduino/), built with PlatformIO and the Arduino 3 framework. All devices join a [Zigbee2MQTT](https://www.zigbee2mqtt.io/) network.

## Projects

| Project | Description |
|---|---|
| `garage-doors` | 4-relay switch controller for garage and yard doors/gates |

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
