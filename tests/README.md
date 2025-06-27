# Unit Testing for main.c

This directory contains unit tests for the ESP32 Bluetooth HFP headset emulator application.

## Overview

The testing strategy uses **host-based unit testing** to test business logic without requiring ESP32 hardware. Key functions from `main.c` have been extracted into separate modules that can be tested independently.

## Architecture

### Extracted Modules

- **`src/config_utils.c`**: Configuration validation and utilities
  - Phone number validation
  - Auto-redial period clamping (10-84600 seconds)
  - WiFi mode enum to string conversion

- **`src/json_utils.c`**: JSON parsing and generation
  - WiFi configuration parsing
  - Auto-redial settings parsing
  - Status response generation

### Test Structure

- **`tests/`**: Host-based unit tests
  - `test_config_utils.c`: Tests for configuration utilities
  - `test_json_utils.c`: Tests for JSON handling
  - `test_runner.c`: Simple test framework and runner
  - `include/simple_test.h`: Test macros and framework

## Running Tests

### Locally

```bash
cd tests
make install-deps  # Install libcjson-dev
make test
```

### In CI/CD

Tests run automatically on pull requests that modify:
- `main/` directory
- `src/` directory 
- `tests/` directory
- `CMakeLists.txt`

## Test Coverage

The tests cover critical business logic:

✅ **Phone number validation** - Ensures only valid phone numbers are accepted  
✅ **Auto-redial configuration** - Period clamping, enable/disable logic  
✅ **JSON parsing** - WiFi credentials, auto-redial settings  
✅ **Response generation** - Status API responses  
✅ **Error handling** - Invalid JSON, missing fields, null parameters  

## Benefits

1. **Fast Feedback**: Tests run in seconds on standard hardware
2. **No Hardware Dependencies**: Tests run on any system with gcc and libcjson
3. **Regression Prevention**: Automated testing on every PR
4. **Documentation**: Tests serve as executable specifications
5. **Refactoring Safety**: Changes can be verified without ESP32 hardware

## Adding New Tests

1. Extract testable function from `main.c` into `src/` module
2. Add corresponding test file in `tests/`
3. Use `REGISTER_TEST()` macro to register test functions
4. Update Makefile if adding new source files

### Example Test

```c
#include "include/simple_test.h"
#include "my_module.h"

REGISTER_TEST(test_my_function_handles_valid_input)
int test_my_function_handles_valid_input(void) {
    int result = my_function("valid input");
    TEST_ASSERT_EQUAL_INT(0, result);
    return 1;
}
```

## Integration with ESP32 Build

The extracted modules are designed to be easily integrated back into the main ESP32 application. The modular approach improves code organization and testability without affecting the main application functionality.