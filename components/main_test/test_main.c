#include "test_utils.h"

void run_all_tests(void)
{
    printf("\n");
    printf("ESP32 Remotehead Unit Tests\n");
    printf("===========================\n");
    
    UNITY_BEGIN();

    // Utility function tests
    printf("Running utility function tests...\n");
    RUN_TEST(test_wifi_mode_string_conversion);
    RUN_TEST(test_phone_number_validation);
    RUN_TEST(test_json_validation);
    RUN_TEST(test_status_response_generation);

    // HTTP handler tests  
    printf("Running HTTP handler tests...\n");
    RUN_TEST(test_http_handler_redial_bluetooth_disconnected);
    RUN_TEST(test_http_handler_dial_valid_number);
    RUN_TEST(test_http_handler_dial_invalid_number);
    RUN_TEST(test_http_handler_status_response);
    RUN_TEST(test_http_handler_configure_wifi_valid_json);
    RUN_TEST(test_http_handler_configure_wifi_invalid_json);
    RUN_TEST(test_http_handler_set_auto_redial_valid);
    RUN_TEST(test_http_handler_set_auto_redial_invalid);

    // NVS operation tests
    printf("Running NVS operation tests...\n");
    RUN_TEST(test_nvs_wifi_credentials_save_load);
    RUN_TEST(test_nvs_auto_redial_settings_save_load);
    RUN_TEST(test_nvs_missing_data_handling);
    RUN_TEST(test_nvs_error_handling);

    printf("All tests completed.\n");
    UNITY_END();
}

void app_main(void)
{
    run_all_tests();
}