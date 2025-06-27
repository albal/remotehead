#include "unity.h"
#include "test_utils.h"
#include "cJSON.h"
#include <string.h>

// Mock function to simulate httpd_resp_send_json behavior
esp_err_t mock_httpd_resp_send_json(httpd_req_t *req, const char *json_str) {
    if (!req || !json_str) return ESP_ERR_INVALID_ARG;
    
    // Validate JSON
    if (validate_json_response(json_str) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simulate setting content type
    if (req->content_type) {
        strcpy(req->content_type, "application/json");
    }
    
    // Simulate storing response
    if (req->response_buffer && req->buffer_size > strlen(json_str)) {
        strcpy(req->response_buffer, json_str);
        return ESP_OK;
    }
    
    return ESP_ERR_NO_MEM;
}

// Test logic for status handler without ESP32 dependencies
esp_err_t create_status_response(bool bluetooth_connected, int wifi_mode,
                                const char* ip_address, bool auto_redial_enabled,
                                uint32_t redial_period, char* response_buffer, size_t buffer_size) {
    cJSON *root = create_status_json(bluetooth_connected, wifi_mode, ip_address, 
                                   auto_redial_enabled, redial_period);
    if (!root) return ESP_ERR_NO_MEM;
    
    const char *json_response = cJSON_PrintUnformatted(root);
    if (!json_response) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    if (strlen(json_response) >= buffer_size) {
        cJSON_Delete(root);
        free((void*)json_response);
        return ESP_ERR_NO_MEM;
    }
    
    strcpy(response_buffer, json_response);
    cJSON_Delete(root);
    free((void*)json_response);
    return ESP_OK;
}

// Test logic for WiFi configuration parsing
esp_err_t parse_wifi_config(const char* json_content, char* ssid, char* password, 
                           size_t ssid_size, size_t password_size) {
    if (!json_content || !ssid || !password) return ESP_ERR_INVALID_ARG;
    
    cJSON *root = cJSON_Parse(json_content);
    if (!root) return ESP_ERR_INVALID_ARG;
    
    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItemCaseSensitive(root, "password");
    
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json) ||
        !ssid_json->valuestring || !password_json->valuestring) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(ssid_json->valuestring) >= ssid_size || 
        strlen(password_json->valuestring) >= password_size) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    strcpy(ssid, ssid_json->valuestring);
    strcpy(password, password_json->valuestring);
    
    cJSON_Delete(root);
    return ESP_OK;
}

// Test logic for auto-redial configuration parsing
esp_err_t parse_auto_redial_config(const char* json_content, bool* enabled, uint32_t* period) {
    if (!json_content || !enabled || !period) return ESP_ERR_INVALID_ARG;
    
    cJSON *root = cJSON_Parse(json_content);
    if (!root) return ESP_ERR_INVALID_ARG;
    
    cJSON *enabled_json = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON *period_json = cJSON_GetObjectItemCaseSensitive(root, "period");
    
    if (!cJSON_IsBool(enabled_json) || !cJSON_IsNumber(period_json)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    *enabled = cJSON_IsTrue(enabled_json);
    *period = (uint32_t)cJSON_GetNumberValue(period_json);
    
    // Clamp period to valid range (as in original code)
    if (*period < 10) *period = 10;
    if (*period > 84600) *period = 84600;
    
    cJSON_Delete(root);
    return ESP_OK;
}

// Test cases for HTTP handler logic

void test_mock_httpd_resp_send_json_handles_valid_json(void) {
    char response_buffer[256];
    char content_type[32];
    httpd_req_t req = {
        .response_buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .content_type = content_type
    };
    
    esp_err_t result = mock_httpd_resp_send_json(&req, "{\"status\": \"ok\"}");
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("{\"status\": \"ok\"}", response_buffer);
    TEST_ASSERT_EQUAL_STRING("application/json", content_type);
}

void test_mock_httpd_resp_send_json_rejects_invalid_json(void) {
    char response_buffer[256];
    httpd_req_t req = {
        .response_buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .content_type = NULL
    };
    
    esp_err_t result = mock_httpd_resp_send_json(&req, "{invalid json");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_create_status_response_generates_correct_json(void) {
    char response_buffer[512];
    
    esp_err_t result = create_status_response(true, WIFI_MODE_STA, "192.168.1.100", 
                                            true, 60, response_buffer, sizeof(response_buffer));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    // Parse the response to validate content
    cJSON *json = cJSON_Parse(response_buffer);
    TEST_ASSERT_NOT_NULL(json);
    
    cJSON *bt_connected = cJSON_GetObjectItem(json, "bluetooth_connected");
    TEST_ASSERT_TRUE(cJSON_IsTrue(bt_connected));
    
    cJSON *wifi_mode = cJSON_GetObjectItem(json, "wifi_mode");
    TEST_ASSERT_EQUAL_STRING("STA", cJSON_GetStringValue(wifi_mode));
    
    cJSON_Delete(json);
}

void test_parse_wifi_config_extracts_ssid_and_password(void) {
    const char* json_content = "{\"ssid\": \"TestNetwork\", \"password\": \"TestPassword\"}";
    char ssid[32], password[64];
    
    esp_err_t result = parse_wifi_config(json_content, ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("TestNetwork", ssid);
    TEST_ASSERT_EQUAL_STRING("TestPassword", password);
}

void test_parse_wifi_config_handles_invalid_json(void) {
    char ssid[32], password[64];
    
    esp_err_t result = parse_wifi_config("{invalid json", ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
    
    result = parse_wifi_config("{\"ssid\": \"test\"}", ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result); // Missing password
}

void test_parse_auto_redial_config_extracts_settings_correctly(void) {
    const char* json_content = "{\"enabled\": true, \"period\": 120}";
    bool enabled;
    uint32_t period;
    
    esp_err_t result = parse_auto_redial_config(json_content, &enabled, &period);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL(120, period);
}

void test_parse_auto_redial_config_clamps_period_values(void) {
    bool enabled;
    uint32_t period;
    
    // Test lower bound
    esp_err_t result = parse_auto_redial_config("{\"enabled\": false, \"period\": 5}", &enabled, &period);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL(10, period); // Should be clamped to minimum
    
    // Test upper bound
    result = parse_auto_redial_config("{\"enabled\": true, \"period\": 100000}", &enabled, &period);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL(84600, period); // Should be clamped to maximum
}