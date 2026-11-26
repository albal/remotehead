# remotehead
A WiFi to Bluetooth bridge allowing you to control dialling or redialling on a mobile phone

## Features
- ESP32-based Bluetooth HFP (Hands-Free Profile) headset emulator
- WiFi AP/STA mode for web-based configuration
- HTTP API for dialing and redial control
- Automatic redial functionality with configurable intervals
- Web interface for easy configuration
- Comprehensive test suite with CI/CD integration

## Releases

Pre-built firmware binaries are available in the [Releases](https://github.com/albal/remotehead/releases) section. Each release includes:
- Complete flashable firmware image
- Individual component binaries
- Flashing instructions
- Checksums for verification

See [RELEASE.md](RELEASE.md) for detailed release process documentation.

## Testing
This project includes extensive unit tests for the core functionality. See [docs/TESTING.md](docs/TESTING.md) for details on running tests locally and the CI/CD pipeline.

Tests are automatically run on pull requests and cover:
- HTTP request handlers
- JSON parsing and validation  
- Configuration management
- NVS storage operations

## Web Installer

Install the latest release from your browser at
**https://albal.github.io/remotehead/**. Plug the ESP32 in over USB, click
**Connect & install** and pick its serial port. It needs Chrome or Edge on a
desktop (Web Serial). The page installs the bootloader, partition table,
firmware and web interface, so it is also the easiest way to do the one-time
USB flash described below.

The site is published by `.github/workflows/pages.yml`. It runs after each
release build, downloads that release's binaries, and generates an
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) manifest whose
flash offsets come from the release's own partition table
(`site/build_manifest.py`).

## Firmware Updates over Wi-Fi

Once a device is running firmware with the dual-slot partition table, later
updates can be installed over Wi-Fi with no USB cable.

**From the web interface:** open the device's page, find **Firmware Update**,
choose `remotehead.bin` from a release (not the complete flash image) and press
**Upload and Install**. The device writes the image to its spare slot, checks
it, and restarts into it.

**From the command line:**

```bash
curl --data-binary @build/remotehead.bin \
  -H "Content-Type: application/octet-stream" \
  http://<device-ip>/update
```

`GET /status` reports the running version as `firmware_version`.

If the new firmware crashes before its web server starts, the bootloader rolls
back to the previous version automatically.

Notes:
- Wi-Fi updates replace the firmware only. The web interface lives in the
  SPIFFS partition, so a release that changes the web UI still needs
  `spiffs.bin` flashed over USB (`esptool.py write_flash 0x390000 spiffs.bin`).
- `/update` has no authentication, like the rest of the API. Anyone who can
  reach the device on your network can install firmware on it.

### One-time USB flash required

Older devices have a single app partition, so they can't take an update over
Wi-Fi. Flash the complete image over USB once to install the new partition
table, bootloader and layout:

| Partition | Offset     | Size    |
|-----------|------------|---------|
| ota_0     | `0x10000`  | 1.75 MB |
| ota_1     | `0x1D0000` | 1.75 MB |
| spiffs    | `0x390000` | 448 KB  |

The flash erases the SPIFFS partition. Wi-Fi credentials and Bluetooth pairings
in NVS are kept.
