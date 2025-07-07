#include "unity.h"
#include "test_utils.h"
#include "test_http_handlers.h"
#include "test_nvs_utils.h"

void app_main(void) {
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
    
    UNITY_END();

    exit(0);
}
