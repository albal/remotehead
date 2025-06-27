#include "json_utils.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>

int parse_wifi_config(const char* json_str, char* ssid, char* password, 
                      size_t ssid_len, size_t password_len) {
    if (!json_str || !ssid || !password) {
        return -1; // Invalid parameters
    }
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1; // Invalid JSON
    }
    
    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItemCaseSensitive(root, "password");
    
    if (!cJSON_IsString(ssid_json) || !ssid_json->valuestring ||
        !cJSON_IsString(password_json) || !password_json->valuestring) {
        cJSON_Delete(root);
        return -1; // Missing or invalid fields
    }
    
    // Copy values with bounds checking
    strncpy(ssid, ssid_json->valuestring, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    
    strncpy(password, password_json->valuestring, password_len - 1);
    password[password_len - 1] = '\0';
    
    cJSON_Delete(root);
    return 0; // Success
}

int parse_auto_redial_config(const char* json_str, int* enabled, uint32_t* period) {
    if (!json_str || !enabled || !period) {
        return -1; // Invalid parameters
    }
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1; // Invalid JSON
    }
    
    cJSON *enabled_json = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON *period_json = cJSON_GetObjectItemCaseSensitive(root, "period");
    
    if (!cJSON_IsBool(enabled_json) || !cJSON_IsNumber(period_json)) {
        cJSON_Delete(root);
        return -1; // Missing or invalid fields
    }
    
    *enabled = cJSON_IsTrue(enabled_json);
    *period = (uint32_t)cJSON_GetNumberValue(period_json);
    
    cJSON_Delete(root);
    return 0; // Success
}

char* create_status_response(int bluetooth_connected, const char* wifi_mode, 
                            const char* ip_address, int auto_redial_enabled, 
                            uint32_t redial_period) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    
    cJSON_AddBoolToObject(root, "bluetooth_connected", bluetooth_connected);
    cJSON_AddStringToObject(root, "wifi_mode", wifi_mode ? wifi_mode : "Unknown");
    cJSON_AddStringToObject(root, "ip_address", ip_address && strlen(ip_address) > 0 ? ip_address : "N/A");
    cJSON_AddBoolToObject(root, "auto_redial_enabled", auto_redial_enabled);
    cJSON_AddNumberToObject(root, "redial_period", redial_period);
    
    const char* message = bluetooth_connected ? 
        "ESP32 Bluetooth connected to phone." : 
        "ESP32 Bluetooth disconnected.";
    cJSON_AddStringToObject(root, "message", message);
    
    char* json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_string; // Caller must free this
}