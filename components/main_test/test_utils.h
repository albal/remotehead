#pragma once

#include "esp_err.h"
#include "cJSON.h"

// Mock HTTP request structure for testing
typedef struct {
    char* response_buffer;
    size_t buffer_size;
    char* content_type;
} httpd_req_t;

// Test-exposed functions from main.c
esp_err_t httpd_resp_send_json(httpd_req_t *req, const char *json_str);

// Utility functions for testing
char* wifi_mode_to_string(int mode);
cJSON* create_status_json(bool bluetooth_connected, int wifi_mode, 
                         const char* ip_address, bool auto_redial_enabled, 
                         uint32_t redial_period);

// Test helper functions
esp_err_t validate_json_response(const char* json_str);
bool is_valid_phone_number(const char* number);