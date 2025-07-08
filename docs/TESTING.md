# Testing Documentation

## Overview

This project includes comprehensive unit tests for the main.c ESP32 application. The tests cover:

- HTTP handler logic
- JSON parsing and validation
- NVS (Non-Volatile Storage) operations
- Utility functions
- Configuration management

## Test Structure

The tests are organized in the `components/main_test/` directory:

- `test_main.c` - Main test runner
- `test_utils.c` - Tests for utility functions
- `test_http_handlers.c` - Tests for HTTP request handlers
- `test_nvs_utils.c` - Tests for NVS storage operations
- `test_utils.h` - Header with test function declarations

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

### Running Tests in CI

Tests are automatically run on pull requests through GitHub Actions. The workflow:

1. Sets up ESP-IDF environment
2. Builds the test project
3. Performs code quality checks
4. Ensures main project still builds with test infrastructure

## Test Coverage

The tests focus on business logic that can be tested without hardware dependencies:

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

## Mocking Strategy

Since many ESP32 functions require hardware, the tests use mocking for:

- HTTP request/response handling
- NVS storage operations
- ESP32-specific APIs

This allows testing the core logic without requiring ESP32 hardware.

## Adding New Tests

1. Add test functions to appropriate test file
2. Follow naming convention: `test_function_name_describes_behavior`
3. Include test function declaration in `test_main.c`
4. Add `RUN_TEST()` call in `app_main()`

Example:

```c
void test_new_function_works_correctly(void) {
    // Test implementation
    TEST_ASSERT_EQUAL(expected, actual);
}
```

## Limitations

- Tests focus on business logic, not hardware integration
- Some ESP32-specific functionality cannot be fully tested without hardware
- Mock implementations may not cover all edge cases of real ESP32 APIs

## Future Improvements

- Add integration tests with ESP32 simulator
- Expand test coverage for Bluetooth functionality
- Add performance benchmarks
- Add automated test result reporting

## Additional Resources

For detailed instructions on building and running tests, refer to the `README.md` file in the `components/main_test/` directory.