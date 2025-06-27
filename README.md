# remotehead
A WiFi to Bluetooth bridge allowing you to control dialling or redialling on a mobile phone

## Features
- ESP32-based Bluetooth HFP (Hands-Free Profile) headset emulator
- WiFi AP/STA mode for web-based configuration
- HTTP API for dialing and redial control
- Automatic redial functionality with configurable intervals
- Web interface for easy configuration
- Comprehensive test suite with CI/CD integration

## Testing
This project includes extensive unit tests for the core functionality. See [docs/TESTING.md](docs/TESTING.md) for details on running tests locally and the CI/CD pipeline.

Tests are automatically run on pull requests and cover:
- HTTP request handlers
- JSON parsing and validation  
- Configuration management
- NVS storage operations
