"""
PlatformIO pre-script: firmware version and Zigbee OTA image.

- Reads the semver from $FIRMWARE_VERSION (set by the release workflow, "0.0.0" for local builds)
  and exposes it as FIRMWARE_VERSION (uint32, 0x00MMmmpp) and FIRMWARE_VERSION_STR.
- When the project sets `custom_ota_image_type`, exposes OTA_MANUFACTURER / OTA_IMAGE_TYPE and wraps
  firmware.bin into firmware.ota (Zigbee OTA Upgrade File, ZCL §11.4) after each build.

Usage in platformio.ini:
    extra_scripts = pre:../../scripts/firmware_version.py
    custom_ota_image_type = 0xD101
"""

import os
import re
import struct

Import("env")  # noqa: F821  (injected by SCons)

OTA_MANUFACTURER = 0x1001  # Arduino Zigbee library default (ZigbeeEP::addOTAClient)
OTA_HEADER_LENGTH = 56     # header without optional fields
OTA_STACK_VERSION = 0x0002  # Zigbee PRO
OTA_TAG_UPGRADE_IMAGE = 0x0000

version = os.environ.get("FIRMWARE_VERSION", "0.0.0")
match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
if not match or any(int(part) > 0xFF for part in match.groups()):
    raise SystemExit(f"FIRMWARE_VERSION must be MAJOR.MINOR.PATCH with parts <= 255, got {version!r}")
major, minor, patch = (int(part) for part in match.groups())
file_version = (major << 16) | (minor << 8) | patch

env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", f"0x{file_version:08X}"),
    ("FIRMWARE_VERSION_STR", env.StringifyMacro(version)),
])

image_type = env.GetProjectOption("custom_ota_image_type", None)
if image_type is not None:
    image_type = int(image_type, 0)
    env.Append(CPPDEFINES=[
        ("OTA_MANUFACTURER", f"0x{OTA_MANUFACTURER:04X}"),
        ("OTA_IMAGE_TYPE", f"0x{image_type:04X}"),
    ])

    def build_ota(source, target, env):
        firmware = target[0].get_abspath()
        ota = os.path.splitext(firmware)[0] + ".ota"
        with open(firmware, "rb") as f:
            payload = f.read()

        name = env.GetProjectConfig().get("platformio", "name", "firmware")
        header_string = f"{name} {version}".encode()[:32].ljust(32, b"\0")
        sub_element = struct.pack("<HI", OTA_TAG_UPGRADE_IMAGE, len(payload)) + payload
        header = struct.pack(
            "<IHHHHHIH32sI",
            0x0BEEF11E,         # upgrade file identifier
            0x0100,             # header version
            OTA_HEADER_LENGTH,
            0x0000,             # field control: no optional fields
            OTA_MANUFACTURER,
            image_type,
            file_version,
            OTA_STACK_VERSION,
            header_string,
            OTA_HEADER_LENGTH + len(sub_element),  # total image size
        )
        with open(ota, "wb") as f:
            f.write(header + sub_element)
        print(f"Built {ota} (manufacturer 0x{OTA_MANUFACTURER:04X}, image type 0x{image_type:04X}, "
              f"file version 0x{file_version:08X})")

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_ota)
