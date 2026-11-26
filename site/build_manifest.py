#!/usr/bin/env python3
"""Write an ESP Web Tools manifest for a downloaded RemoteHead release.

Usage: build_manifest.py <firmware_dir> <version> <output_manifest>

<firmware_dir> holds the release's bootloader.bin, partition-table.bin,
remotehead.bin, spiffs.bin and (newer releases) ota_data_initial.bin.

Flash offsets are read from that release's own partition-table.bin, so the
manifest is correct for both the old single-app layout and the dual-OTA one.
"""
import json
import os
import struct
import sys

BOOTLOADER_OFFSET = 0x1000      # ESP32 (classic)
PARTITION_TABLE_OFFSET = 0x8000

ENTRY = struct.Struct("<2sBBII16sI")
MAGIC = b"\xaa\x50"

TYPE_APP, TYPE_DATA = 0x00, 0x01
SUBTYPE_FACTORY, SUBTYPE_OTA_0 = 0x00, 0x10
SUBTYPE_OTADATA, SUBTYPE_SPIFFS = 0x00, 0x82


def read_partitions(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = []
    for i in range(0, len(data) - ENTRY.size + 1, ENTRY.size):
        magic, ptype, subtype, offset, size, label, _flags = ENTRY.unpack_from(data, i)
        if magic != MAGIC:
            break  # MD5 entry (0xEBEB) or end of table (0xFFFF)
        parts.append({
            "type": ptype,
            "subtype": subtype,
            "offset": offset,
            "size": size,
            "label": label.rstrip(b"\x00").decode(),
        })
    return parts


def find(parts, ptype, subtypes):
    for sub in subtypes:
        for p in parts:
            if p["type"] == ptype and p["subtype"] == sub:
                return p
    return None


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    fw_dir, version, out_path = sys.argv[1:]
    rel = os.path.relpath(fw_dir, os.path.dirname(os.path.abspath(out_path)))

    def need(name):
        if not os.path.isfile(os.path.join(fw_dir, name)):
            sys.exit(f"missing {name} in {fw_dir}")
        return f"{rel}/{name}"

    parts = read_partitions(os.path.join(fw_dir, "partition-table.bin"))
    app = find(parts, TYPE_APP, [SUBTYPE_OTA_0, SUBTYPE_FACTORY])
    spiffs = find(parts, TYPE_DATA, [SUBTYPE_SPIFFS])
    otadata = find(parts, TYPE_DATA, [SUBTYPE_OTADATA])
    if app is None or spiffs is None:
        sys.exit("partition table has no app or spiffs partition")

    flash = [
        {"path": need("bootloader.bin"), "offset": BOOTLOADER_OFFSET},
        {"path": need("partition-table.bin"), "offset": PARTITION_TABLE_OFFSET},
    ]
    # Reset otadata so the device boots the freshly written ota_0 even if it
    # had previously switched to ota_1 through a Wi-Fi update.
    if otadata and os.path.isfile(os.path.join(fw_dir, "ota_data_initial.bin")):
        flash.append({"path": need("ota_data_initial.bin"), "offset": otadata["offset"]})
    flash.append({"path": need("remotehead.bin"), "offset": app["offset"]})
    flash.append({"path": need("spiffs.bin"), "offset": spiffs["offset"]})

    manifest = {
        "name": "RemoteHead",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32", "parts": flash}],
    }
    with open(out_path, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    for p in flash:
        print(f"  0x{p['offset']:06x}  {p['path']}")
    print(f"wrote {out_path} for {version}")


if __name__ == "__main__":
    main()
