"""
Builds a Zigbee2MQTT OTA index (ota.zigbee_ota_override_index_location) from release OTA images.

Usage: ota_index.py <index.json> <repo url> <tag.ota>...
  Each image is named after its release tag and served from <repo url>/releases/download/<tag>/<tag>.ota.
  Remote entries must carry the header fields: z2m only parses local files.
"""

import hashlib
import json
import os
import sys

from zigpy.ota.image import parse_ota_image

index_path, repo_url, *images = sys.argv[1:]

index = []
for path in images:
    with open(path, "rb") as f:
        data = f.read()
    header = parse_ota_image(data)[0].header

    file_name = os.path.basename(path)
    tag = file_name.removesuffix(".ota")
    index.append({
        "fileName": file_name,
        "url": f"{repo_url}/releases/download/{tag}/{file_name}",
        "fileVersion": header.file_version,
        "fileSize": len(data),
        "manufacturerCode": header.manufacturer_id,
        "imageType": header.image_type,
        "otaHeaderString": header.header_string.rstrip(b"\0").decode(),
        "sha512": hashlib.sha512(data).hexdigest(),
        "releaseNotes": f"{repo_url}/releases/tag/{tag}",
    })
    print(f"{file_name}: image type 0x{header.image_type:04X}, file version 0x{header.file_version:08X}")

with open(index_path, "w") as f:
    json.dump(index, f, indent=2)
    f.write("\n")
