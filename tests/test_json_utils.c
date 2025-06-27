#include "include/simple_test.h"
#include "json_utils.h"
#include <stdlib.h>
#include <string.h>

REGISTER_TEST(test_parse_wifi_config_extracts_ssid_and_password)
int test_parse_wifi_config_extracts_ssid_and_password(void) {
    const char* json = "{\"ssid\": \"TestNetwork\", \"password\": \"TestPassword\"}";
    char ssid[32], password[64];
    
    int result = parse_wifi_config(json, ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING("TestNetwork", ssid);
    TEST_ASSERT_EQUAL_STRING("TestPassword", password);
    return 1;
}

REGISTER_TEST(test_parse_wifi_config_handles_invalid_json)
int test_parse_wifi_config_handles_invalid_json(void) {
    char ssid[32], password[64];
    
    // Invalid JSON
    TEST_ASSERT_EQUAL_INT(-1, parse_wifi_config("{invalid json", ssid, password, sizeof(ssid), sizeof(password)));
    
    // Missing fields
    TEST_ASSERT_EQUAL_INT(-1, parse_wifi_config("{\"ssid\": \"test\"}", ssid, password, sizeof(ssid), sizeof(password)));
    
    // Null parameters
    TEST_ASSERT_EQUAL_INT(-1, parse_wifi_config(NULL, ssid, password, sizeof(ssid), sizeof(password)));
    TEST_ASSERT_EQUAL_INT(-1, parse_wifi_config("{}", NULL, password, sizeof(ssid), sizeof(password)));
    
    return 1;
}

REGISTER_TEST(test_parse_wifi_config_truncates_long_values)
int test_parse_wifi_config_truncates_long_values(void) {
    const char* json = "{\"ssid\": \"VeryLongNetworkNameThatExceedsBuffer\", \"password\": \"VeryLongPasswordThatExceedsBufferSize\"}";
    char ssid[10], password[10];
    
    int result = parse_wifi_config(json, ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(9, strlen(ssid)); // Should be truncated to buffer size - 1
    TEST_ASSERT_EQUAL_INT(9, strlen(password));
    return 1;
}

REGISTER_TEST(test_parse_auto_redial_config_extracts_values)
int test_parse_auto_redial_config_extracts_values(void) {
    const char* json = "{\"enabled\": true, \"period\": 120}";
    int enabled;
    uint32_t period;
    
    int result = parse_auto_redial_config(json, &enabled, &period);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_INT(120, period);
    return 1;
}

REGISTER_TEST(test_parse_auto_redial_config_handles_disabled)
int test_parse_auto_redial_config_handles_disabled(void) {
    const char* json = "{\"enabled\": false, \"period\": 60}";
    int enabled;
    uint32_t period;
    
    int result = parse_auto_redial_config(json, &enabled, &period);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_FALSE(enabled);
    TEST_ASSERT_EQUAL_INT(60, period);
    return 1;
}

REGISTER_TEST(test_parse_auto_redial_config_handles_invalid_json)
int test_parse_auto_redial_config_handles_invalid_json(void) {
    int enabled;
    uint32_t period;
    
    // Invalid JSON
    TEST_ASSERT_EQUAL_INT(-1, parse_auto_redial_config("{invalid", &enabled, &period));
    
    // Wrong types
    TEST_ASSERT_EQUAL_INT(-1, parse_auto_redial_config("{\"enabled\": \"true\", \"period\": 60}", &enabled, &period));
    TEST_ASSERT_EQUAL_INT(-1, parse_auto_redial_config("{\"enabled\": true, \"period\": \"60\"}", &enabled, &period));
    
    return 1;
}

REGISTER_TEST(test_create_status_response_generates_valid_json)
int test_create_status_response_generates_valid_json(void) {
    char* response = create_status_response(1, "STA", "192.168.1.100", 1, 60);
    TEST_ASSERT_NOT_NULL(response);
    
    // Should contain expected fields (basic check)
    TEST_ASSERT_TRUE(strstr(response, "bluetooth_connected") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "wifi_mode") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "ip_address") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "auto_redial_enabled") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "redial_period") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "message") != NULL);
    
    free(response);
    return 1;
}

REGISTER_TEST(test_create_status_response_handles_null_values)
int test_create_status_response_handles_null_values(void) {
    char* response = create_status_response(0, NULL, NULL, 0, 30);
    TEST_ASSERT_NOT_NULL(response);
    
    // Should handle null gracefully
    TEST_ASSERT_TRUE(strstr(response, "Unknown") != NULL);
    TEST_ASSERT_TRUE(strstr(response, "N/A") != NULL);
    
    free(response);
    return 1;
}