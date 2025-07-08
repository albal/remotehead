#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Forward declarations to avoid including complex headers
typedef struct httpd_req httpd_req_t;

// Function prototypes from main.c that need to be tested

// NVS functions
bool load_wifi_credentials_from_nvs(char *ssid, char *password, size_t ssid_len, size_t password_len);
void save_wifi_credentials_to_nvs(const char *ssid, const char *password);
bool load_auto_redial_settings_from_nvs(void);
void save_auto_redial_settings_to_nvs(bool enabled, uint32_t period, uint32_t random_delay, uint32_t max_count);

// HTTP handlers (with forward declarations)
esp_err_t redial_get_handler(httpd_req_t *req);
esp_err_t dial_get_handler(httpd_req_t *req);
esp_err_t status_get_handler(httpd_req_t *req);
esp_err_t configure_wifi_post_handler(httpd_req_t *req);
esp_err_t set_auto_redial_post_handler(httpd_req_t *req);
esp_err_t serve_static_file(httpd_req_t *req);

// Utility functions
void url_decode(char *str);

// Other functions that might be needed for testing
void auto_redial_timer_callback(void* arg);

#endif // MAIN_H
