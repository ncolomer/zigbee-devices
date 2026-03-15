# Agent Guidelines for Zigbee Firmware Development

## Quick Reference

**Target Hardware:** Seeed Studio XIAO ESP32C6
**Build Tool:** PlatformIO (pio)
**Framework:** Arduino 3
**Platform:** https://github.com/pioarduino/platform-espressif32
**Board:** `seeed_xiao_esp32c6`

**Key Rules:**
- Keep code simple, not defensive (DIY philosophy)
- Keep documentation concise and minimal
- Comments only where they add value
- Battery devices: deep sleep mandatory
- All firmwares: OTA support required
- Devices join Zigbee2MQTT network (external converters needed)
- **Test all changes**: Always run `pio run` after code or config changes

## Repository Context

This repository contains **Zigbee firmware projects** targeting the **Seeed Studio XIAO ESP32C6** module. All projects use PlatformIO (pio) as the build tool and for firmware uploads.

## Build System & Framework

- **Build Tool**: PlatformIO (pio)
- **Framework**: Arduino 3
- **Platform**: Use the pioarduino platform: `https://github.com/pioarduino/platform-espressif32`
- **Board**: `seeed_xiao_esp32c6`

**Note:** Always use the pioarduino platform, not the standard espressif32 platform.

## Essential Documentation

**Always reference these documentation sources for context:**

1. **Seeed Studio XIAO ESP32C6 Zigbee Arduino Guide**: https://wiki.seeedstudio.com/xiao_esp32c6_zigbee_arduino/
2. **Using Cursor to Create Zigbee Projects**: https://wiki.seeedstudio.com/use_cursor_create_zigbee_prj/
3. **Espressif Arduino Zigbee API (high-level)**: https://docs.espressif.com/projects/arduino-esp32/en/latest/zigbee/zigbee.html
4. **Espressif ESP-Zigbee SDK (low-level stack reference)**: https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/api-reference/index.html

## Code Examples & Inspiration

**Systematically search and reference examples from these repositories:**

1. **PlatformIO ESP32 Examples**: https://github.com/pioarduino/platform-espressif32/tree/main/examples
   - Look for Zigbee-related examples, deep sleep patterns, and OTA implementations

2. **XIAO ESP32C6 Sketches**: https://github.com/sigmdel/xiao_esp32c6_sketches
   - Contains practical examples for the XIAO ESP32C6 board, including Zigbee implementations

3. **Seeed Studio Platform Boards**: https://github.com/Seeed-Studio/platform-seeedboards
   - Platform definitions and board support for Seeed Studio hardware

4. **Espressif Arduino ESP32**: https://github.com/espressif/arduino-esp32
   - Official Arduino framework for ESP32, includes Zigbee library examples and implementations

Use the **GitHub MCP** to search and fetch code from these repositories. Adapt patterns from official examples when implementing features.

## Code Philosophy

**This is DIY firmware, not commercial-grade code:**

- **Do NOT over-engineer** - Keep code simple and straightforward
- **Do NOT add excessive defensive programming** - Focus on the happy path
- **Handle main edge cases** - Only add error handling that helps with debugging and understanding issues
- **Prioritize clarity** - Code should be easy to read and understand
- **Add helpful debug output** - Use conditional compilation for debug builds to aid troubleshooting
- **No licensing headers** - Do not include license notices or copyright headers in source files

## Documentation & Comments

**Keep documentation concise and minimal:**

- **Markdown documentation** - Keep README files concise, focus on essential information only
- **Code comments** - Add comments only in relevant places where they add value:
  - Explain non-obvious logic or design decisions
  - Document workarounds or platform-specific quirks
  - Clarify complex algorithms or calculations
  - **Do NOT** comment obvious code or repeat what the code already clearly shows

## Power Management Requirements

**For battery-powered firmwares:**

- **Deep sleep is mandatory** - Devices must deep sleep as much as possible to maximize battery life
- **Wake-up strategies**:
  - Use external interrupts for event-driven wake-ups (e.g., sensor pulses, button presses)
  - Use RTC timers only when necessary for periodic tasks
  - Minimize active time - process events quickly and return to sleep
- **RTC memory** - Use `RTC_DATA_ATTR` for variables that need to persist across deep sleep cycles
- **Zigbee sleepy end device mode** - Configure devices as sleepy end devices (ED) to minimize power consumption
- **Optimize Zigbee reporting** - Report periodically rather than on every event to balance power vs. connectivity

## OTA (Over-The-Air) Updates

**All firmwares MUST support OTA updates:**

- Enable OTA functionality in PlatformIO configuration
- Use Zigbee OTA cluster for firmware updates over the Zigbee network
- Ensure OTA updates work reliably even for battery-powered devices
- Test OTA update process during development
- Document OTA update procedure in project README

**OTA Flash Requirements:**
- OTA uses internal flash memory - no external flash IC required
- XIAO ESP32C6 has 4MB internal flash, which is sufficient for OTA with typical firmware sizes
- OTA requires partitioning flash into two application slots (ota_0, ota_1) plus OTA data partition
- The `zigbee.csv` partition scheme supports OTA using internal flash
- If firmware grows very large (>1.5MB), consider using an ESP32 module with 8MB+ flash

## Project Structure

Each firmware project should follow this structure:
```
project-name/
├── platformio.ini
├── src/
│   └── main.cpp
└── z2m-external-converter/   # Zigbee2MQTT external converter (if needed)
```

Shared libraries live in `libraries/` at the repo root and are referenced via `lib_extra_dirs = ../libraries` in each project's `platformio.ini`.

## PlatformIO Configuration Guidelines

- Use `default_envs = debug` for development
- Create separate `debug` and `release` environments
- In `release` builds: disable serial output, optimize for size and power
- In `debug` builds: enable serial logging, USB CDC, and debug macros
- Always set `ZIGBEE_MODE_ED` flag for end device mode
- Configure appropriate partition schemes for OTA support

## Zigbee Implementation Guidelines

- Use the official ESP32 Zigbee library from the Arduino framework
- Follow patterns from official Zigbee examples
- Configure devices as **End Devices (ED)** for battery-powered applications
- Use appropriate Zigbee clusters for sensor/actuator types:
  - Analog Input for numeric sensors
  - Binary Input/Output for switches
  - Flow Sensor for water/flow meters
  - Temperature/Humidity sensors as appropriate
- Set power source attributes correctly (battery vs. mains)
- Configure reporting intervals appropriately for battery life

## Zigbee2MQTT Integration

**Network Context:**
- Custom firmwares will join a Zigbee network managed by **Zigbee2MQTT**
- Devices must be compatible with Zigbee2MQTT's discovery and management

**External Converters:**
- Zigbee messages from custom devices will require **external converters** for proper interpretation
- External converters documentation: https://www.zigbee2mqtt.io/advanced/more/external_converters.html
- Converters map Zigbee cluster attributes to MQTT topics and Home Assistant entities

**Converter Development:**
- Reference existing converters for patterns and structure: https://github.com/Koenkk/zigbee-herdsman-converters/tree/master/src/devices
- Study similar device types to understand:
  - Cluster usage patterns
  - Attribute reporting configurations
  - Device type definitions
  - Expose patterns for Home Assistant integration
- When implementing firmware, consider how attributes will be exposed via converters
- Ensure device exposes standard Zigbee clusters that converters can interpret

## Debugging & Development

- Use the shared `libraries/Debug/Debug.h` library for debug output:
  - `DEBUG_INIT()` / `DEBUG_END()` — initialize/teardown serial
  - `DEBUG_PRINT(...)` / `DEBUG_PRINTLN(...)` — printf-style logging, compiled out in release builds
- Serial output should only be enabled in debug builds
- Add meaningful debug messages that help understand device behavior
- Include wake-up reason logging in debug builds
- Log Zigbee connection status and sensor updates

## Common Patterns

### Deep Sleep with External Wake-up
```cpp
// Configure external wake-up
esp_sleep_enable_ext1_wakeup(PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
esp_deep_sleep_start();
```

### RTC Memory Persistence
```cpp
RTC_DATA_ATTR uint32_t counter = 0;  // Persists across deep sleep
```

### NVS Persistent Storage
```cpp
#include <Preferences.h>
Preferences nvs;

nvs.begin("namespace", false);
uint16_t val = nvs.getUShort("key", defaultValue);
nvs.putUShort("key", val);
nvs.end();
```

## Testing & Validation

**Mandatory Testing:**
- **All code and PlatformIO config changes MUST be tested** using `pio run` to verify compilation succeeds
- Test both `debug` and `release` build environments: `pio run -e debug` and `pio run -e release`
- Verify no compilation errors or warnings before considering changes complete

**Considerations for Implementation:**
- Keep in mind that deep sleep wake-up functionality will need manual testing
- Consider Zigbee connectivity and reporting patterns during development
- Be aware that OTA update process will be validated manually
- For battery-powered devices, consider power consumption implications in code design
- Design code to handle edge cases that may arise in real-world deployment

## Documentation Requirements

**Keep project README files concise and minimal.** Each project should include only essential information:
- Hardware requirements and connections
- Configuration options
- Build and deployment instructions
- OTA update procedure
- Troubleshooting section (only common issues)
- Power consumption notes (for battery-powered devices only)

**Avoid verbose explanations or redundant information.**
