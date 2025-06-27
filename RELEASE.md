# Release Process

This repository includes an automated release pipeline that builds and attaches ESP32 firmware binaries to GitHub releases.

## How to Create a Release

1. **Create a new release** on GitHub:
   - Go to the [Releases page](https://github.com/albal/remotehead/releases)
   - Click "Create a new release"
   - Choose or create a new tag (e.g., `v1.0.0`, `v1.1.0`)
   - Add a release title and description
   - Click "Publish release"

2. **Automatic build process**:
   - The release workflow (`.github/workflows/release.yml`) will automatically trigger
   - It builds the React web application and ESP32 firmware
   - All firmware binaries are attached to the release

## What Gets Attached to Releases

Each release will include the following files:

### Firmware Binaries
- `bootloader.bin` - ESP32 bootloader
- `partition-table.bin` - Partition table for flash memory layout
- `remotehead.bin` - Main application firmware
- `spiffs.bin` - SPIFFS filesystem image containing the web interface
- `esp32-redialer-{version}-complete.bin` - Combined flashable image (recommended)

### Documentation
- `FLASHING_INSTRUCTIONS.md` - Detailed instructions for flashing the firmware
- `checksums.txt` - SHA256 checksums for verifying file integrity

## For End Users

If you just want to flash the firmware to your ESP32:

1. Download `esp32-redialer-{version}-complete.bin` from the latest release
2. Download `checksums.txt` and verify the file integrity
3. Follow the instructions in `FLASHING_INSTRUCTIONS.md`

## Development Notes

- The release process is based on the existing `build.yml` workflow
- React app is built and embedded into the SPIFFS filesystem
- ESP-IDF v5.1 is used for the firmware build
- The combined binary includes all components at the correct flash offsets