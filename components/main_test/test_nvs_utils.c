#include "unity.h"
#include "test_utils.h"
#include <string.h>

// Mock NVS key-value storage for testing
#define MAX_NVS_ENTRIES 10
typedef struct {
    char key[32];
    char value[128];
    bool is_string;
    bool is_bool;
    bool is_uint32;
    uint32_t uint32_value;
    bool bool_value;
} mock_nvs_entry_t;

static mock_nvs_entry_t mock_nvs_storage[MAX_NVS_ENTRIES];
static int nvs_entry_count = 0;

// Mock NVS functions for testing
esp_err_t mock_nvs_get_str(const char* namespace_name, const char* key, char* out_value, size_t* length) {
    for (int i = 0; i < nvs_entry_count; i++) {
        if (strcmp(mock_nvs_storage[i].key, key) == 0 && mock_nvs_storage[i].is_string) {
            if (*length <= strlen(mock_nvs_storage[i].value)) {
                return ESP_ERR_NVS_INVALID_LENGTH;
            }
            strcpy(out_value, mock_nvs_storage[i].value);
            *length = strlen(mock_nvs_storage[i].value);
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t mock_nvs_set_str(const char* namespace_name, const char* key, const char* value) {
    // Find existing entry or create new one
    int index = -1;
    for (int i = 0; i < nvs_entry_count; i++) {
        if (strcmp(mock_nvs_storage[i].key, key) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        if (nvs_entry_count >= MAX_NVS_ENTRIES) return ESP_ERR_NO_MEM;
        index = nvs_entry_count++;
    }
    
    strcpy(mock_nvs_storage[index].key, key);
    strcpy(mock_nvs_storage[index].value, value);
    mock_nvs_storage[index].is_string = true;
    mock_nvs_storage[index].is_bool = false;
    mock_nvs_storage[index].is_uint32 = false;
    return ESP_OK;
}

esp_err_t mock_nvs_get_u32(const char* namespace_name, const char* key, uint32_t* out_value) {
    for (int i = 0; i < nvs_entry_count; i++) {
        if (strcmp(mock_nvs_storage[i].key, key) == 0 && mock_nvs_storage[i].is_uint32) {
            *out_value = mock_nvs_storage[i].uint32_value;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t mock_nvs_set_u32(const char* namespace_name, const char* key, uint32_t value) {
    int index = -1;
    for (int i = 0; i < nvs_entry_count; i++) {
        if (strcmp(mock_nvs_storage[i].key, key) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        if (nvs_entry_count >= MAX_NVS_ENTRIES) return ESP_ERR_NO_MEM;
        index = nvs_entry_count++;
    }
    
    strcpy(mock_nvs_storage[index].key, key);
    mock_nvs_storage[index].uint32_value = value;
    mock_nvs_storage[index].is_string = false;
    mock_nvs_storage[index].is_bool = false;
    mock_nvs_storage[index].is_uint32 = true;
    return ESP_OK;
}

// Helper function to clear mock NVS storage
void clear_mock_nvs(void) {
    nvs_entry_count = 0;
    memset(mock_nvs_storage, 0, sizeof(mock_nvs_storage));
}

// Test functions for WiFi credential storage/loading logic
esp_err_t test_save_wifi_credentials(const char* ssid, const char* password) {
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = mock_nvs_set_str("redial_config", "ssid", ssid);
    if (err != ESP_OK) return err;
    
    err = mock_nvs_set_str("redial_config", "password", password);
    return err;
}

esp_err_t test_load_wifi_credentials(char* ssid, char* password, size_t ssid_len, size_t password_len) {
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = mock_nvs_get_str("redial_config", "ssid", ssid, &ssid_len);
    if (err != ESP_OK) return err;
    
    err = mock_nvs_get_str("redial_config", "password", password, &password_len);
    return err;
}

// Test functions for auto-redial settings
esp_err_t test_save_auto_redial_settings(bool enabled, uint32_t period) {
    esp_err_t err = mock_nvs_set_u32("redial_config", "auto_en", enabled ? 1 : 0);
    if (err != ESP_OK) return err;
    
    err = mock_nvs_set_u32("redial_config", "redial_period", period);
    return err;
}

esp_err_t test_load_auto_redial_settings(bool* enabled, uint32_t* period) {
    if (!enabled || !period) return ESP_ERR_INVALID_ARG;
    
    uint32_t enabled_val;
    esp_err_t err = mock_nvs_get_u32("redial_config", "auto_en", &enabled_val);
    if (err != ESP_OK) return err;
    
    err = mock_nvs_get_u32("redial_config", "redial_period", period);
    if (err != ESP_OK) return err;
    
    *enabled = (enabled_val != 0);
    return ESP_OK;
}

// Test cases for NVS functionality

void test_wifi_credentials_can_be_saved_and_loaded(void) {
    clear_mock_nvs();
    
    const char* test_ssid = "TestNetwork";
    const char* test_password = "TestPassword123";
    
    // Save credentials
    esp_err_t result = test_save_wifi_credentials(test_ssid, test_password);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    // Load credentials
    char loaded_ssid[32];
    char loaded_password[64];
    result = test_load_wifi_credentials(loaded_ssid, loaded_password, sizeof(loaded_ssid), sizeof(loaded_password));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(test_ssid, loaded_ssid);
    TEST_ASSERT_EQUAL_STRING(test_password, loaded_password);
}

void test_wifi_credential_loading_fails_when_not_stored(void) {
    clear_mock_nvs();
    
    char ssid[32];
    char password[64];
    esp_err_t result = test_load_wifi_credentials(ssid, password, sizeof(ssid), sizeof(password));
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, result);
}

void test_auto_redial_settings_can_be_saved_and_loaded(void) {
    clear_mock_nvs();
    
    bool test_enabled = true;
    uint32_t test_period = 120;
    
    // Save settings
    esp_err_t result = test_save_auto_redial_settings(test_enabled, test_period);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    // Load settings
    bool loaded_enabled;
    uint32_t loaded_period;
    result = test_load_auto_redial_settings(&loaded_enabled, &loaded_period);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL(test_enabled, loaded_enabled);
    TEST_ASSERT_EQUAL(test_period, loaded_period);
}

void test_auto_redial_loading_fails_when_not_stored(void) {
    clear_mock_nvs();
    
    bool enabled;
    uint32_t period;
    esp_err_t result = test_load_auto_redial_settings(&enabled, &period);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, result);
}

void test_nvs_functions_handle_null_parameters(void) {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_save_wifi_credentials(NULL, "password"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_save_wifi_credentials("ssid", NULL));
    
    char buffer[32];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_load_wifi_credentials(NULL, buffer, 32, 32));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_load_wifi_credentials(buffer, NULL, 32, 32));
    
    bool enabled;
    uint32_t period;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_load_auto_redial_settings(NULL, &period));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_load_auto_redial_settings(&enabled, NULL));
}