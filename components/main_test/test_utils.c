#include "unity.h"
#include "test_utils.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include <string.h>

// Test utility functions

char* wifi_mode_to_string(int mode) {
    switch(mode) {
        case WIFI_MODE_AP: return "AP";
        case WIFI_MODE_STA: return "STA";
        case WIFI_MODE_APSTA: return "APSTA";
        default: return "Unknown";
    }
}

cJSON* create_status_json(bool bluetooth_connected, int wifi_mode, 
                         const char* ip_address, bool auto_redial_enabled, 
                         uint32_t redial_period) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "bluetooth_connected", bluetooth_connected);
    cJSON_AddStringToObject(root, "wifi_mode", wifi_mode_to_string(wifi_mode));
    cJSON_AddStringToObject(root, "ip_address", 
                           (ip_address && strlen(ip_address) > 0) ? ip_address : "N/A");
    cJSON_AddBoolToObject(root, "auto_redial_enabled", auto_redial_enabled);
    cJSON_AddNumberToObject(root, "redial_period", redial_period);
    cJSON_AddStringToObject(root, "message", 
                           bluetooth_connected ? "ESP32 Bluetooth connected to phone." 
                                              : "ESP32 Bluetooth disconnected.");
    return root;
}

esp_err_t validate_json_response(const char* json_str) {
    if (!json_str) return ESP_ERR_INVALID_ARG;
    
    cJSON *json = cJSON_Parse(json_str);
    if (!json) return ESP_ERR_INVALID_ARG;
    
    cJSON_Delete(json);
    return ESP_OK;
}

bool is_valid_phone_number(const char* number) {
    if (!number || strlen(number) == 0) return false;
    if (strlen(number) > 20) return false; // Reasonable max length
    
    // Check for valid characters (digits, +, -, space, parentheses)
    for (int i = 0; i < strlen(number); i++) {
        char c = number[i];
        if (!(c >= '0' && c <= '9') && c != '+' && c != '-' && 
            c != ' ' && c != '(' && c != ')') {
            return false;
        }
    }
    return true;
}

// Test cases for utility functions
void test_wifi_mode_to_string_converts_modes_correctly(void) {
    TEST_ASSERT_EQUAL_STRING("AP", wifi_mode_to_string(WIFI_MODE_AP));
    TEST_ASSERT_EQUAL_STRING("STA", wifi_mode_to_string(WIFI_MODE_STA));
    TEST_ASSERT_EQUAL_STRING("APSTA", wifi_mode_to_string(WIFI_MODE_APSTA));
    TEST_ASSERT_EQUAL_STRING("Unknown", wifi_mode_to_string(999));
}

void test_create_status_json_creates_valid_json(void) {
    cJSON *json = create_status_json(true, WIFI_MODE_STA, "192.168.1.100", true, 60);
    TEST_ASSERT_NOT_NULL(json);
    
    cJSON *bt_connected = cJSON_GetObjectItem(json, "bluetooth_connected");
    TEST_ASSERT_TRUE(cJSON_IsTrue(bt_connected));
    
    cJSON *wifi_mode = cJSON_GetObjectItem(json, "wifi_mode");
    TEST_ASSERT_EQUAL_STRING("STA", cJSON_GetStringValue(wifi_mode));
    
    cJSON *ip_addr = cJSON_GetObjectItem(json, "ip_address");
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", cJSON_GetStringValue(ip_addr));
    
    cJSON_Delete(json);
}

void test_validate_json_response_validates_json_strings(void) {
    TEST_ASSERT_EQUAL(ESP_OK, validate_json_response("{\"test\": \"value\"}"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, validate_json_response("{invalid json"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, validate_json_response(NULL));
}

void test_is_valid_phone_number_validates_phone_numbers(void) {
    TEST_ASSERT_TRUE(is_valid_phone_number("1234567890"));
    TEST_ASSERT_TRUE(is_valid_phone_number("+1-234-567-8900"));
    TEST_ASSERT_TRUE(is_valid_phone_number("(555) 123-4567"));
    TEST_ASSERT_FALSE(is_valid_phone_number(""));
    TEST_ASSERT_FALSE(is_valid_phone_number(NULL));
    TEST_ASSERT_FALSE(is_valid_phone_number("invalid-number!"));
    TEST_ASSERT_FALSE(is_valid_phone_number("123456789012345678901")); // Too long
}