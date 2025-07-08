# Unit Testing for remotehead (ESP32)

## How to Build and Run Tests

The test project is separate from the main firmware build. To run the unit tests:

1. **Navigate to the test directory:**
   ```sh
   cd test
   ```

2. **Build the test project:**
   ```sh
   idf.py build
   ```

3. **Flash and run the tests on your ESP32:**
   ```sh
   idf.py flash monitor
   ```

   The test runner will execute all tests and print results to the serial monitor.

---

## How to Build the Main Firmware

From the project root directory:

```sh
idf.py build
idf.py flash monitor
```

This will build and flash the main application, not the tests.

---

## Test Structure

The tests are organized in the `test/main/` directory:

- `test_main.c` - Main test runner
- `test_utils.c` - Tests for utility functions like `url_decode`
- `test_http_handlers.c` - Mock tests for HTTP request handlers
- `test_nvs_utils.c` - Mock tests for NVS storage operations
- `test_utils.h` - Header with test function declarations

## Notes

- The test project is isolated from the main firmware. Tests are run from the `test` directory.
- Current tests include actual testing of the `url_decode` function and mock tests for other components.
- See `docs/TESTING.md` for more details on test structure and coverage.
