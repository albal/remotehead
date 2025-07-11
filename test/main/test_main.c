#include <stdint.h>
#include <stdlib.h>
#include "unity.h"
#include "test_utils.h"
#include "test_http_handlers.h"
#include "test_nvs_utils.h"
#include "test_audio_streaming.h"

/**
 * @brief Tells the QEMU emulator to exit with a success status code.
 */
static void qemu_exit_success(void)
{
    // The address of the QEMU power-off device
    volatile uint32_t *addr = (volatile uint32_t *)0x3FF00004;
    // Write the magic value to trigger a successful power-off
    *addr = 1;
}

/**
 * @brief Tells the QEMU emulator to exit with a failure status code.
 */
static void qemu_exit_failure(void)
{
    volatile uint32_t *addr = (volatile uint32_t *)0x3FF00004;
    // Write a different value (e.g., 3) for failure. QEMU exits with status (val >> 1).
    *addr = 3;
}

void app_main(void)
{
    UNITY_BEGIN();

    // Utility tests
    RUN_TEST(test_url_decode_basic);
    RUN_TEST(test_url_decode_plus_sign);
    RUN_TEST(test_url_decode_hex_chars);
    RUN_TEST(test_basic_string_validation);

    // HTTP handler tests
    RUN_TEST(test_http_handler_mock);
    RUN_TEST(test_request_validation_mock);

    // NVS tests
    RUN_TEST(test_nvs_mock);
    RUN_TEST(test_settings_persistence_mock);

    // Audio streaming tests
    RUN_TEST(test_websocket_handler_basic);
    RUN_TEST(test_hfp_audio_data_callback);
    RUN_TEST(test_audio_callback_registration);
    RUN_TEST(test_websocket_endpoint_config);
    RUN_TEST(test_audio_data_format);
    RUN_TEST(test_audio_buffer_management);
    RUN_TEST(test_websocket_client_handling);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_websocket_frame_types);
    RUN_TEST(test_audio_streaming_integration);

    // UNITY_END() returns the number of failures.
    int failures = UNITY_END();

    // Pass the failure count to the official exit function.
    // This will exit QEMU with a status of 0 for success or 1 for failure.
    test_utils_finish(failures);
}
