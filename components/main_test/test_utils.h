#pragma once

#include "unity.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

// Main test runner function
void run_all_tests(void);

// Test function declarations
void test_wifi_mode_string_conversion(void);
void test_phone_number_validation(void);
void test_json_validation(void);
void test_status_response_generation(void);

void test_http_handler_redial_bluetooth_disconnected(void);
void test_http_handler_dial_valid_number(void);
void test_http_handler_dial_invalid_number(void);
void test_http_handler_status_response(void);
void test_http_handler_configure_wifi_valid_json(void);
void test_http_handler_configure_wifi_invalid_json(void);
void test_http_handler_set_auto_redial_valid(void);
void test_http_handler_set_auto_redial_invalid(void);

void test_nvs_wifi_credentials_save_load(void);
void test_nvs_auto_redial_settings_save_load(void);
void test_nvs_missing_data_handling(void);
void test_nvs_error_handling(void);

// Mock HTTP request structure for testing
typedef struct {
    const char* query_string;
    const char* content;
    size_t content_len;
    char response_buffer[1024];
    size_t response_len;
    const char* content_type;
} mock_httpd_req_t;

// Mock functions
esp_err_t mock_httpd_resp_sendstr(httpd_req_t *req, const char* str);
esp_err_t mock_httpd_resp_set_type(httpd_req_t *req, const char* type);