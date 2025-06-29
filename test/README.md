# ESP32 Remotehead Unit Tests

This directory contains the unit test suite for the ESP32 Remotehead application.

## Running Tests Locally

### Prerequisites

1. ESP-IDF v5.1 or later installed
2. ESP-IDF environment sourced (`get_idf` or `. $HOME/esp/esp-idf/export.sh`)

### Build and Run Tests

```bash
# Navigate to test directory
cd test

# Build the test project
idf.py build

# Flash and monitor (if you have ESP32 hardware)
idf.py flash monitor

# Or run in QEMU simulator (if available)
idf.py qemu
```

## Test Structure

The tests are organized in the `components/main_test/` directory:

- `test_main.c` - Main test runner
- `test_utils.c` - Tests for utility functions
- `test_http_handlers.c` - Tests for HTTP request handlers
- `test_nvs_utils.c` - Tests for NVS storage operations
- `test_utils.h` - Header with test function declarations

## Test Coverage

The tests cover:

### HTTP Handlers
- JSON response generation
- Request parameter parsing
- Configuration validation
- Error handling

### NVS Operations
- WiFi credential storage/retrieval
- Auto-redial settings persistence
- Error handling for missing data

### Utilities
- WiFi mode string conversion
- Phone number validation
- JSON validation
- Status response generation

## CI/CD Integration

Tests are automatically run on pull requests through GitHub Actions in the `test-firmware.yml` workflow.