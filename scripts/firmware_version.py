"""
PlatformIO pre-script: firmware version and Zigbee OTA identifiers as build defines.

- FIRMWARE_VERSION_STR / FIRMWARE_VERSION: $FIRMWARE_VERSION ("0.0.0" for local builds) and its Zigbee OTA
  file version, one byte per part (0.1.2 -> 0x00000102). The release workflow wraps the same numbers into the .ota.
- OTA_MANUFACTURER / OTA_IMAGE_TYPE: `custom_ota_manufacturer` / `custom_ota_image_type`, when set.
"""

import os

Import("env")  # noqa: F821  (injected by SCons)

version = os.environ.get("FIRMWARE_VERSION", "0.0.0")
major, minor, patch = (int(part) for part in version.split("."))

env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", f"0x{(major << 16) | (minor << 8) | patch:08X}"),
    ("FIRMWARE_VERSION_STR", env.StringifyMacro(version)),
])

for define, option in (("OTA_MANUFACTURER", "custom_ota_manufacturer"), ("OTA_IMAGE_TYPE", "custom_ota_image_type")):
    value = env.GetProjectOption(option, None)
    if value is not None:
        env.Append(CPPDEFINES=[(define, value)])
