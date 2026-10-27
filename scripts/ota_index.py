"""
Builds a Zigbee2MQTT OTA index (ota.zigbee_ota_override_index_location) from release OTA images.

Usage: ota_index.py <index.json> <repo url> <tag.ota>...
  Each image is named after its release tag and served from <repo url>/releases/download/<tag>/<tag>.ota.
  Remote entries must carry the header fields: z2m only parses local files.
"""

import hashlib
import json
import os
import struct
import sys

index_path, repo_url, *images = sys.argv[1:]

index = []
for path in images:
    with open(path, "rb") as f:
        data = f.read()
    magic, _, _, _, manufacturer, image_type, file_version, _, header_string, _ = struct.unpack_from("<IHHHHHIH32sI", data)
    assert magic == 0x0BEEF11E, f"{path} is not a Zigbee OTA image"

    file_name = os.path.basename(path)
    tag = file_name.removesuffix(".ota")
    index.append({
        "fileName": file_name,
        "url": f"{repo_url}/releases/download/{tag}/{file_name}",
        "fileVersion": file_version,
        "fileSize": len(data),
        "manufacturerCode": manufacturer,
        "imageType": image_type,
        "otaHeaderString": header_string.rstrip(b"\0").decode(),
        "sha512": hashlib.sha512(data).hexdigest(),
        "releaseNotes": f"{repo_url}/releases/tag/{tag}",
    })
    print(f"{file_name}: image type 0x{image_type:04X}, file version 0x{file_version:08X}")

with open(index_path, "w") as f:
    json.dump(index, f, indent=2)
    f.write("\n")
