#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_log.h"

// Define test functions from other files
extern void test_wifi_mode_to_string_converts_modes_correctly(void);
extern void test_create_status_json_creates_valid_json(void);
extern void test_validate_json_response_validates_json_strings(void);
extern void test_is_valid_phone_number_validates_phone_numbers(void);
extern void test_mock_httpd_resp_send_json_handles_valid_json(void);
extern void test_mock_httpd_resp_send_json_rejects_invalid_json(void);
extern void test_create_status_response_generates_correct_json(void);
extern void test_parse_wifi_config_extracts_ssid_and_password(void);
extern void test_parse_wifi_config_handles_invalid_json(void);
extern void test_parse_auto_redial_config_extracts_settings_correctly(void);
extern void test_parse_auto_redial_config_clamps_period_values(void);
extern void test_wifi_credentials_can_be_saved_and_loaded(void);
extern void test_wifi_credential_loading_fails_when_not_stored(void);
extern void test_auto_redial_settings_can_be_saved_and_loaded(void);
extern void test_auto_redial_loading_fails_when_not_stored(void);
extern void test_nvs_functions_handle_null_parameters(void);

// Test runner for all tests
void app_main(void)
{
    ESP_LOGI("TEST", "Starting main.c unit tests");
    
    UNITY_BEGIN();
    
    // Run utility tests
    ESP_LOGI("TEST", "Running utility tests...");
    RUN_TEST(test_wifi_mode_to_string_converts_modes_correctly);
    RUN_TEST(test_create_status_json_creates_valid_json);
    RUN_TEST(test_validate_json_response_validates_json_strings);
    RUN_TEST(test_is_valid_phone_number_validates_phone_numbers);
    
    // Run HTTP handler tests
    ESP_LOGI("TEST", "Running HTTP handler tests...");
    RUN_TEST(test_mock_httpd_resp_send_json_handles_valid_json);
    RUN_TEST(test_mock_httpd_resp_send_json_rejects_invalid_json);
    RUN_TEST(test_create_status_response_generates_correct_json);
    RUN_TEST(test_parse_wifi_config_extracts_ssid_and_password);
    RUN_TEST(test_parse_wifi_config_handles_invalid_json);
    RUN_TEST(test_parse_auto_redial_config_extracts_settings_correctly);
    RUN_TEST(test_parse_auto_redial_config_clamps_period_values);
    
    // Run NVS tests
    ESP_LOGI("TEST", "Running NVS tests...");
    RUN_TEST(test_wifi_credentials_can_be_saved_and_loaded);
    RUN_TEST(test_wifi_credential_loading_fails_when_not_stored);
    RUN_TEST(test_auto_redial_settings_can_be_saved_and_loaded);
    RUN_TEST(test_auto_redial_loading_fails_when_not_stored);
    RUN_TEST(test_nvs_functions_handle_null_parameters);
    
    UNITY_END();
}