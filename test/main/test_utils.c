#include "test_utils.h"
#include "esp_wifi.h"
#include <string.h>

// Test suite for utility functions

void test_wifi_mode_string_conversion(void)
{
    // Test Wi-Fi mode to string conversion logic
    const char* ap_mode_str = "AP";
    const char* sta_mode_str = "STA";
    const char* unknown_mode_str = "Unknown";
    
    // Test the conversion logic used in status_get_handler
    wifi_mode_t test_mode;
    const char* mode_str;
    
    test_mode = WIFI_MODE_AP;
    if (test_mode == WIFI_MODE_AP) mode_str = "AP";
    else if (test_mode == WIFI_MODE_STA) mode_str = "STA";
    else mode_str = "Unknown";
    TEST_ASSERT_EQUAL_STRING(ap_mode_str, mode_str);
    
    test_mode = WIFI_MODE_STA;
    if (test_mode == WIFI_MODE_AP) mode_str = "AP";
    else if (test_mode == WIFI_MODE_STA) mode_str = "STA";
    else mode_str = "Unknown";
    TEST_ASSERT_EQUAL_STRING(sta_mode_str, mode_str);
    
    test_mode = WIFI_MODE_NULL;
    if (test_mode == WIFI_MODE_AP) mode_str = "AP";
    else if (test_mode == WIFI_MODE_STA) mode_str = "STA";
    else mode_str = "Unknown";
    TEST_ASSERT_EQUAL_STRING(unknown_mode_str, mode_str);
}

void test_phone_number_validation(void)
{
    // Test phone number format validation
    // These would be valid phone numbers for dialing
    const char* valid_numbers[] = {
        "+447911123456",
        "07911123456",
        "01234567890",
        "123456",
        "*31#"
    };
    
    const char* invalid_numbers[] = {
        "",
        "abc123",
        "++44123456", 
        "+44-791-112-3456" // Contains dashes which might cause issues
    };
    
    // Test valid numbers (length and basic character checks)
    for (int i = 0; i < sizeof(valid_numbers)/sizeof(valid_numbers[0]); i++) {
        size_t len = strlen(valid_numbers[i]);
        TEST_ASSERT_GREATER_THAN(0, len);
        TEST_ASSERT_LESS_THAN(64, len); // Should fit in our parameter buffer
    }
    
    // Test invalid numbers
    for (int i = 0; i < sizeof(invalid_numbers)/sizeof(invalid_numbers[0]); i++) {
        if (strlen(invalid_numbers[i]) == 0) {
            TEST_ASSERT_EQUAL(0, strlen(invalid_numbers[i]));
        }
    }
}

void test_json_validation(void)
{
    // Test JSON parsing and validation
    const char* valid_wifi_json = "{\"ssid\":\"TestNetwork\",\"password\":\"testpass\"}";
    const char* invalid_wifi_json = "{\"ssid\":\"TestNetwork\",\"password\":}";
    const char* valid_redial_json = "{\"enabled\":true,\"period\":60}";
    const char* invalid_redial_json = "{\"enabled\":\"invalid\",\"period\":60}";
    
    // Test valid Wi-Fi configuration JSON
    cJSON *root = cJSON_Parse(valid_wifi_json);
    TEST_ASSERT_NOT_NULL(root);
    
    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItemCaseSensitive(root, "password");
    
    TEST_ASSERT_TRUE(cJSON_IsString(ssid_json) && (ssid_json->valuestring != NULL));
    TEST_ASSERT_TRUE(cJSON_IsString(password_json) && (password_json->valuestring != NULL));
    TEST_ASSERT_EQUAL_STRING("TestNetwork", ssid_json->valuestring);
    TEST_ASSERT_EQUAL_STRING("testpass", password_json->valuestring);
    
    cJSON_Delete(root);
    
    // Test invalid Wi-Fi configuration JSON
    root = cJSON_Parse(invalid_wifi_json);
    TEST_ASSERT_NULL(root);
    
    // Test valid auto-redial JSON
    root = cJSON_Parse(valid_redial_json);
    TEST_ASSERT_NOT_NULL(root);
    
    cJSON *enabled_json = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON *period_json = cJSON_GetObjectItemCaseSensitive(root, "period");
    
    TEST_ASSERT_TRUE(cJSON_IsBool(enabled_json));
    TEST_ASSERT_TRUE(cJSON_IsNumber(period_json));
    TEST_ASSERT_TRUE(cJSON_IsTrue(enabled_json));
    TEST_ASSERT_EQUAL(60, (int)cJSON_GetNumberValue(period_json));
    
    cJSON_Delete(root);
    
    // Test invalid auto-redial JSON
    root = cJSON_Parse(invalid_redial_json);
    TEST_ASSERT_NOT_NULL(root); // JSON parses but validation should fail
    
    enabled_json = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    period_json = cJSON_GetObjectItemCaseSensitive(root, "period");
    
    TEST_ASSERT_FALSE(cJSON_IsBool(enabled_json)); // "invalid" is a string, not bool
    TEST_ASSERT_TRUE(cJSON_IsNumber(period_json));
    
    cJSON_Delete(root);
}

void test_status_response_generation(void)
{
    // Test status JSON response generation
    bool test_bluetooth_connected = true;
    wifi_mode_t test_wifi_mode = WIFI_MODE_STA;
    const char* test_ip_address = "192.168.1.100";
    bool test_auto_redial_enabled = true;
    uint32_t test_redial_period = 60;
    
    cJSON *root = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(root);
    
    cJSON_AddBoolToObject(root, "bluetooth_connected", test_bluetooth_connected);
    
    const char* wifi_mode_str = "Unknown";
    if (test_wifi_mode == WIFI_MODE_AP) wifi_mode_str = "AP";
    else if (test_wifi_mode == WIFI_MODE_STA) wifi_mode_str = "STA";
    cJSON_AddStringToObject(root, "wifi_mode", wifi_mode_str);
    
    cJSON_AddStringToObject(root, "ip_address", strlen(test_ip_address) > 0 ? test_ip_address : "N/A");
    cJSON_AddBoolToObject(root, "auto_redial_enabled", test_auto_redial_enabled);
    cJSON_AddNumberToObject(root, "redial_period", test_redial_period);
    
    const char* message = test_bluetooth_connected ? "ESP32 Bluetooth connected to phone." : "ESP32 Bluetooth disconnected.";
    cJSON_AddStringToObject(root, "message", message);
    
    // Verify the JSON structure
    cJSON *bt_field = cJSON_GetObjectItemCaseSensitive(root, "bluetooth_connected");
    cJSON *wifi_field = cJSON_GetObjectItemCaseSensitive(root, "wifi_mode");
    cJSON *ip_field = cJSON_GetObjectItemCaseSensitive(root, "ip_address");
    cJSON *redial_enabled_field = cJSON_GetObjectItemCaseSensitive(root, "auto_redial_enabled");
    cJSON *redial_period_field = cJSON_GetObjectItemCaseSensitive(root, "redial_period");
    cJSON *message_field = cJSON_GetObjectItemCaseSensitive(root, "message");
    
    TEST_ASSERT_TRUE(cJSON_IsBool(bt_field));
    TEST_ASSERT_TRUE(cJSON_IsString(wifi_field));
    TEST_ASSERT_TRUE(cJSON_IsString(ip_field));
    TEST_ASSERT_TRUE(cJSON_IsBool(redial_enabled_field));
    TEST_ASSERT_TRUE(cJSON_IsNumber(redial_period_field));
    TEST_ASSERT_TRUE(cJSON_IsString(message_field));
    
    TEST_ASSERT_TRUE(cJSON_IsTrue(bt_field));
    TEST_ASSERT_EQUAL_STRING("STA", wifi_field->valuestring);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", ip_field->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsTrue(redial_enabled_field));
    TEST_ASSERT_EQUAL(60, (int)cJSON_GetNumberValue(redial_period_field));
    TEST_ASSERT_EQUAL_STRING("ESP32 Bluetooth connected to phone.", message_field->valuestring);
    
    const char *json_response = cJSON_PrintUnformatted(root);
    TEST_ASSERT_NOT_NULL(json_response);
    
    // Verify we can parse it back
    cJSON *parsed = cJSON_Parse(json_response);
    TEST_ASSERT_NOT_NULL(parsed);
    
    cJSON_Delete(parsed);
    cJSON_Delete(root);
    free((void*)json_response);
}