#include <stdio.h>
#include <string.h>
#include <sys/stat.h> // For stat()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "lwip/ip_addr.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_spiffs.h" // For SPIFFS file system

#define TAG "HFP_REDIAL_API"

// Helper function to send JSON response
static esp_err_t httpd_resp_send_json(httpd_req_t *req, const char *json_str) {
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json_str);
}

// --- Global Variables ---
bool is_bluetooth_connected = false; // Bluetooth HFP connection status
httpd_handle_t server = NULL; // HTTP server handle
char current_ip_address[IP4ADDR_STRLEN_MAX]; // To store current IP address
wifi_mode_t current_wifi_mode = WIFI_MODE_NULL; // To store current Wi-Fi mode
esp_netif_t *ap_netif = NULL; // AP network interface handle
esp_netif_t *sta_netif = NULL; // STA network interface handle

// Auto Redial Settings
bool auto_redial_enabled = false;
uint32_t redial_period_seconds = 60; // Default to 60 seconds
uint32_t redial_random_delay_seconds = 0; // New: random delay in seconds
uint32_t last_random_delay_used = 0; // New: last random value used

// Timer handle for automatic redial
esp_timer_handle_t auto_redial_timer;

// Morse code LED task handle
TaskHandle_t morse_code_task_handle = NULL;

// NVS Namespace and Keys
#define NVS_NAMESPACE "redial_config"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_AUTO_REDIAL_ENABLED "auto_en"
#define NVS_KEY_REDIAL_PERIOD "redial_period"
#define NVS_KEY_AUTO_REDIAL_RANDOM "redial_rand"

// AP Mode Configuration
#define AP_SSID "REMOTEHEAD"
#define AP_PASSWORD "" // No password
#define AP_MAX_CONN 4 // Max connections to AP

// GPIO Pin for Factory Reset (D13 on many ESP32 boards)
#define FACTORY_RESET_PIN GPIO_NUM_13

// GPIO Pin for builtin LED (GPIO2 on most ESP32 boards)
#define BUILTIN_LED_PIN GPIO_NUM_2

// Morse code timing (milliseconds)
#define MORSE_DOT_DURATION 200
#define MORSE_DASH_DURATION 600
#define MORSE_SYMBOL_PAUSE 200
#define MORSE_CHAR_PAUSE 600
#define MORSE_IP_READOUT_PAUSE 5000

// SPIFFS Mount Point
#define WEB_MOUNT_POINT "/spiffs"

// File serving constants
#define FILE_PATH_MAX 1024
#define CHUNK_SIZE 1024

// --- Forward Declarations ---
static void esp_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t redial_get_handler(httpd_req_t *req);
static esp_err_t dial_get_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t configure_wifi_post_handler(httpd_req_t *req);
static esp_err_t set_auto_redial_post_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);
static void stop_webserver(httpd_handle_t server);
static void start_wifi_ap(void);
static void start_wifi_sta(const char *ssid, const char *password);
static bool load_wifi_credentials_from_nvs(char *ssid, char *password, size_t ssid_len, size_t password_len);
static void save_wifi_credentials_to_nvs(const char *ssid, const char *password);
static bool load_auto_redial_settings_from_nvs(void);
static void save_auto_redial_settings_to_nvs(bool enabled, uint32_t period, uint32_t random_delay);
void auto_redial_timer_callback(void* arg);
static void update_auto_redial_timer(void);
static esp_err_t serve_static_file(httpd_req_t *req); // New static file server handler
static void morse_code_led_task(void *pvParameters);
static void morse_dot(void);
static void morse_dash(void);
static void morse_digit(char digit);
static void morse_ip_address(const char* ip_addr);
static void init_led_gpio(void);
static void signal_ip_change(void);
static void url_decode(char *str);

// --- Helper function to decode URL-encoded strings ---
static void url_decode(char *str) {
    char *p_str = str;
    char *p_decoded = str;
    while (*p_str) {
        if (*p_str == '%') {
            if (p_str[1] && p_str[2]) {
                // Read the two hex digits that follow '%'
                char hex_buf[3] = { p_str[1], p_str[2], '\0' };
                // Convert the hex value to a character
                *p_decoded++ = (char)strtol(hex_buf, NULL, 16);
                p_str += 3; // Move the source pointer past the encoded part (e.g., past "%2C")
            }
        } else if (*p_str == '+') {
            // A '+' in a URL represents a space
            *p_decoded++ = ' ';
            p_str++;
        } else {
            // Copy the character as-is
            *p_decoded++ = *p_str++;
        }
    }
    *p_decoded = '\0'; // Null-terminate the new, shorter string
}

// --- HFP Client Callback ---
static void esp_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "HFP_CLIENT_EVT: %d", event);

    switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "HFP Client Connected to phone!");
                is_bluetooth_connected = true;
                update_auto_redial_timer(); // Update timer state
            } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "HFP Client Disconnected from phone!");
                is_bluetooth_connected = false;
                update_auto_redial_timer(); // Update timer state
            } else {
                ESP_LOGE(TAG, "HFP Client Connection failed! State: %d", param->conn_stat.state);
            }
            break;
        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "HFP Audio State: %d", param->audio_stat.state);
            break;
        case ESP_HF_CLIENT_BVRA_EVT:
            ESP_LOGI(TAG, "Voice recognition event received");
            break;
        case ESP_HF_CLIENT_CIND_CALL_EVT:
        case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
        case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT:
            ESP_LOGI(TAG, "Call indicator status update received");
            break;
        default:
            ESP_LOGI(TAG, "Unhandled HFP event: %d", event);
            break;
    }
}

// Bluetooth GAP callback
static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "authentication success: %s", param->auth_cmpl.device_name);
                esp_log_buffer_hex(TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            } else {
                ESP_LOGE(TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT: {
            ESP_LOGI(TAG, "ESP_BT_GAP_PIN_REQ_EVT");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1'; pin_code[1] = '2'; pin_code[2] = '3'; pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            break;
        }
#ifdef CONFIG_BT_SSP_ENABLED
        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %lu", param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%lu", param->key_notif.passkey);
            break;
        case ESP_BT_GAP_KEY_REQ_EVT:
            ESP_LOGI(TAG, "ESP_BT_GAP_KEY_REQ_EVT");
            esp_bt_gap_ssp_passkey_reply(param->key_req.bda, true, 0);
            break;
#endif
        default: {
            ESP_LOGI(TAG, "GAP EVT: %d", event);
            break;
        }
    }
    return;
}

// --- NVS Functions ---
static bool load_wifi_credentials_from_nvs(char *ssid, char *password, size_t ssid_len, size_t password_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    size_t required_size_ssid = ssid_len;
    size_t required_size_password = password_len;

    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &required_size_ssid);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error (%s) reading SSID from NVS!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, password, &required_size_password);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error (%s) reading Password from NVS!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND || strlen(ssid) == 0) {
        ESP_LOGI(TAG, "Wi-Fi credentials not found in NVS.");
        return false;
    }

    ESP_LOGI(TAG, "Loaded Wi-Fi credentials: SSID=%s", ssid);
    return true;
}

static void save_wifi_credentials_to_nvs(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for writing!", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing SSID to NVS!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "SSID saved to NVS: %s", ssid);
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing Password to NVS!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Password saved to NVS.");
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS changes!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

static bool load_auto_redial_settings_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for auto redial!", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_u32(nvs_handle, NVS_KEY_REDIAL_PERIOD, &redial_period_seconds);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Redial period not found in NVS, using default.");
        redial_period_seconds = 60; // Default
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) reading redial period from NVS!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    uint8_t enabled_u8 = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_AUTO_REDIAL_ENABLED, &enabled_u8);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Auto redial enabled flag not found in NVS, using default (false).");
        auto_redial_enabled = false; // Default
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) reading auto redial enabled flag from NVS!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    } else {
        auto_redial_enabled = (enabled_u8 != 0);
    }

    // New: Load random delay
    err = nvs_get_u32(nvs_handle, NVS_KEY_AUTO_REDIAL_RANDOM, &redial_random_delay_seconds);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        redial_random_delay_seconds = 0;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) reading redial random delay from NVS!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded auto redial settings: Enabled=%s, Period=%lu seconds, RandomDelay=%lu seconds",
             auto_redial_enabled ? "true" : "false", redial_period_seconds, redial_random_delay_seconds);
    return true;
}

static void save_auto_redial_settings_to_nvs(bool enabled, uint32_t period, uint32_t random_delay) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for auto redial writing!", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(nvs_handle, NVS_KEY_AUTO_REDIAL_ENABLED, enabled ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing auto redial enabled to NVS!", esp_err_to_name(err));
    }

    err = nvs_set_u32(nvs_handle, NVS_KEY_REDIAL_PERIOD, period);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing redial period to NVS!", esp_err_to_name(err));
    }

    // New: Save random delay
    err = nvs_set_u32(nvs_handle, NVS_KEY_AUTO_REDIAL_RANDOM, random_delay);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing redial random delay to NVS!", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS auto redial changes!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Saved auto redial settings: Enabled=%s, Period=%lu seconds, RandomDelay=%lu seconds",
             enabled ? "true" : "false", period, random_delay);
}


// --- Wi-Fi Event Handler ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "Wi-Fi AP started. Connect to SSID: %s", AP_SSID);
            current_wifi_mode = WIFI_MODE_AP;
            strcpy(current_ip_address, "192.168.4.1"); // Default AP IP
            signal_ip_change(); // Signal morse code task about IP change
            if (server == NULL) {
                server = start_webserver();
            }
            update_auto_redial_timer(); // Update timer state
        } else if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi STA started. Connecting...");
            current_wifi_mode = WIFI_MODE_STA;
            esp_wifi_connect();
            update_auto_redial_timer(); // Update timer state
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi STA disconnected. Retrying connection...");
            esp_wifi_connect(); // Attempt to reconnect
            memset(current_ip_address, 0, sizeof(current_ip_address)); // Clear IP on disconnect
            signal_ip_change(); // Signal morse code task about IP change
            update_auto_redial_timer(); // Update timer state
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        ip4addr_ntoa_r((const ip4_addr_t*)&event->ip_info.ip, current_ip_address, sizeof(current_ip_address));
        signal_ip_change(); // Signal morse code task about IP change
        current_wifi_mode = WIFI_MODE_STA;
        if (server == NULL) {
            server = start_webserver(); // Start web server once IP is obtained
        }
        update_auto_redial_timer(); // Update timer state
    }
}

// --- Wi-Fi Initialization Functions ---
static void start_wifi_ap(void) {
    // Create AP interface only if it doesn't already exist
    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1, // Default channel
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN, // No password
            .ssid_hidden = 0, // SSID visible
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void start_wifi_sta(const char *ssid, const char *password) {
    if (current_wifi_mode == WIFI_MODE_AP) {
        // If currently in AP mode, stop it first
        ESP_LOGI(TAG, "Stopping AP mode before switching to STA.");
        ESP_ERROR_CHECK(esp_wifi_stop());
        
        // Properly destroy AP interface if it exists
        if (ap_netif != NULL) {
            esp_netif_destroy(ap_netif);
            ap_netif = NULL;
        }
        
        // Small delay to ensure clean interface transition
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Create STA interface only if it doesn't already exist
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta(); // Create STA interface
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Default to WPA2_PSK, adjust if needed
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH, // Optional: for WPA3
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}


// --- HTTP Server Handlers ---

// Handler for /redial endpoint
static esp_err_t redial_get_handler(httpd_req_t *req)
{
    if (!is_bluetooth_connected) {
        httpd_resp_send_json(req, "{\"error\":\"Bluetooth not connected to phone\"}");
        return ESP_FAIL;
    }
    if (current_wifi_mode != WIFI_MODE_STA) {
        httpd_resp_send_json(req, "{\"error\":\"Device not in STA mode, cannot redial\"}");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HTTP: Received /redial command.");
    esp_hf_client_dial(NULL); // Send the redial command - NULL

    httpd_resp_send_json(req, "{\"message\":\"Redial command sent\"}");
    return ESP_OK;
}

// Handler for /dial?number=<num> endpoint
static esp_err_t dial_get_handler(httpd_req_t *req)
{
    if (!is_bluetooth_connected) {
        httpd_resp_send_json(req, "{\"error\":\"Bluetooth not connected to phone\"}");
        return ESP_FAIL;
    }
    if (current_wifi_mode != WIFI_MODE_STA) {
        httpd_resp_send_json(req, "{\"error\":\"Device not in STA mode, cannot dial\"}");
        return ESP_FAIL;
    }

    char* buf;
    size_t buf_len;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Query: %s", buf);
            char param[64]; // Increased buffer for number parameter to allow longer strings
            memset(param, 0, sizeof(param)); // Clear buffer

            if (httpd_query_key_value(buf, "number", param, sizeof(param)) == ESP_OK) {
                url_decode(param);
                ESP_LOGI(TAG, "HTTP: Received /dial command for number: %s", param);
                esp_hf_client_dial(param); // Send the dial command with the number
                free(buf);
                httpd_resp_send_json(req, "{\"message\":\"Dial command sent\"}");
                return ESP_OK;
            }
        }
        free(buf);
    }

    httpd_resp_send_json(req, "{\"error\":\"Invalid or missing 'number' parameter\"}");
    return ESP_FAIL;
}

// Handler for /status endpoint
static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "bluetooth_connected", is_bluetooth_connected);

    const char* wifi_mode_str = "Unknown";
    if (current_wifi_mode == WIFI_MODE_AP) wifi_mode_str = "AP";
    else if (current_wifi_mode == WIFI_MODE_STA) wifi_mode_str = "STA";
    cJSON_AddStringToObject(root, "wifi_mode", wifi_mode_str);

    cJSON_AddStringToObject(root, "ip_address", strlen(current_ip_address) > 0 ? current_ip_address : "N/A");

    cJSON_AddBoolToObject(root, "auto_redial_enabled", auto_redial_enabled);
    cJSON_AddNumberToObject(root, "redial_period", redial_period_seconds);
    cJSON_AddNumberToObject(root, "redial_random_delay", redial_random_delay_seconds); // New
    cJSON_AddNumberToObject(root, "last_random_delay", last_random_delay_used); // New

    cJSON_AddStringToObject(root, "message", is_bluetooth_connected ? "ESP32 Bluetooth connected to phone." : "ESP32 Bluetooth disconnected.");

    const char *json_response = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json_response);
    cJSON_Delete(root);
    free((void*)json_response); // Free the string allocated by cJSON_PrintUnformatted
    return ESP_OK;
}

// Handler for /configure_wifi POST endpoint
static esp_err_t configure_wifi_post_handler(httpd_req_t *req)
{
    char content_buffer[256];
    int ret = httpd_req_recv(req, content_buffer, sizeof(content_buffer) - 1); // -1 for null terminator
    if (ret <= 0) {  // 0 means connection closed, < 0 means error
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content_buffer[ret] = '\0';

    cJSON *root = cJSON_Parse(content_buffer);
    if (!root) {
        httpd_resp_send_json(req, "{\"error\":\"Invalid JSON format.\"}\n");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (cJSON_IsString(ssid_json) && (ssid_json->valuestring != NULL) &&
        cJSON_IsString(password_json) && (password_json->valuestring != NULL)) {

        save_wifi_credentials_to_nvs(ssid_json->valuestring, password_json->valuestring);

        ESP_LOGI(TAG, "Switching to STA mode with SSID: %s", ssid_json->valuestring);
        stop_webserver(server); // Stop server before Wi-Fi mode change
        server = NULL; // Clear server handle
        start_wifi_sta(ssid_json->valuestring, password_json->valuestring); // Start STA mode

        cJSON_Delete(root);
        httpd_resp_send_json(req, "{\"message\":\"Wi-Fi credentials received and device is attempting to connect to home network.\"}\n");
        return ESP_OK;

    } else {
        cJSON_Delete(root);
        httpd_resp_send_json(req, "{\"error\":\"Missing or invalid 'ssid' or 'password' in JSON.\"}\n");
        return ESP_FAIL;
    }
}

// Handler for /set_auto_redial POST endpoint
static esp_err_t set_auto_redial_post_handler(httpd_req_t *req)
{
    char content_buffer[128];
    int ret = httpd_req_recv(req, content_buffer, sizeof(content_buffer) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content_buffer[ret] = '\0';

    cJSON *root = cJSON_Parse(content_buffer);
    if (!root) {
        httpd_resp_send_json(req, "{\"error\":\"Invalid JSON format.\"}\n");
        return ESP_FAIL;
    }

    cJSON *enabled_json = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON *period_json = cJSON_GetObjectItemCaseSensitive(root, "period");
    cJSON *random_json = cJSON_GetObjectItemCaseSensitive(root, "random_delay"); // New

    if (cJSON_IsBool(enabled_json) && cJSON_IsNumber(period_json)) {
        auto_redial_enabled = cJSON_IsTrue(enabled_json);
        redial_period_seconds = (uint32_t)cJSON_GetNumberValue(period_json);
        if (cJSON_IsNumber(random_json)) {
            redial_random_delay_seconds = (uint32_t)cJSON_GetNumberValue(random_json);
        }

        // Clamp period to valid range
        if (redial_period_seconds < 10) redial_period_seconds = 10;
        if (redial_period_seconds > 84600) redial_period_seconds = 84600;
        if (redial_random_delay_seconds > 86400) redial_random_delay_seconds = 86400;

        save_auto_redial_settings_to_nvs(auto_redial_enabled, redial_period_seconds, redial_random_delay_seconds);
        update_auto_redial_timer(); // Update timer based on new settings

        cJSON_Delete(root);
        httpd_resp_send_json(req, "{\"message\":\"Automatic redial settings updated.\"}\n");
        return ESP_OK;
    } else {
        cJSON_Delete(root);
        httpd_resp_send_json(req, "{\"error\":\"Missing or invalid 'enabled' or 'period' in JSON.\"}\n");
        return ESP_FAIL;
    }
}

// --- Static File Server Handler ---
static esp_err_t serve_static_file(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    const char *filename = req->uri;

    // If URI is just "/", serve index.html
    if (strcmp(req->uri, "/") == 0) {
        filename = "/index.html";
    }

    snprintf(filepath, sizeof(filepath), "%s%s", WEB_MOUNT_POINT, filename);

    struct stat file_stat;
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        /* Respond with 404 Error */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    // Determine content type based on file extension
    if (strstr(filename, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(filename, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(filename, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filename, ".png")) {
        httpd_resp_set_type(req, "image/png");
    } else if (strstr(filename, ".ico")) {
        httpd_resp_set_type(req, "image/x-icon");
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
    }

    char *chunk = (char *)malloc(CHUNK_SIZE);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate memory for chunk");
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_FAIL;
    }

    size_t read_bytes;
    do {
        read_bytes = fread(chunk, 1, CHUNK_SIZE, fd);
        httpd_resp_send_chunk(req, chunk, read_bytes);
    } while (read_bytes > 0);

    free(chunk);
    fclose(fd);
    ESP_LOGI(TAG, "File served: %s", filepath);
    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}


// --- HTTP Server Configuration and Start/Stop ---
static httpd_uri_t redial_uri = {
    .uri       = "/redial",
    .method    = HTTP_GET,
    .handler   = redial_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t dial_uri = {
    .uri       = "/dial",
    .method    = HTTP_GET,
    .handler   = dial_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t status_uri = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t configure_wifi_uri = {
    .uri       = "/configure_wifi",
    .method    = HTTP_POST,
    .handler   = configure_wifi_post_handler,
    .user_ctx  = NULL
};

static httpd_uri_t set_auto_redial_uri = {
    .uri       = "/set_auto_redial",
    .method    = HTTP_POST,
    .handler   = set_auto_redial_post_handler,
    .user_ctx  = NULL
};

// New URI handler for serving static files (catch-all)
static httpd_uri_t static_files_uri = {
    .uri       = "/*", // Matches any URI
    .method    = HTTP_GET,
    .handler   = serve_static_file,
    .user_ctx  = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 6; // Increased to accommodate new handler (root is handled by static_files_uri)
    config.stack_size = 8192; // Increase stack size for HTTP server task if needed
    config.recv_wait_timeout = 10; // Increase timeout for receiving data
    config.send_wait_timeout = 10; // Increase timeout for sending data


    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        // Register API handlers first so they take precedence
        httpd_register_uri_handler(server, &redial_uri);
        httpd_register_uri_handler(server, &dial_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &configure_wifi_uri);
        httpd_register_uri_handler(server, &set_auto_redial_uri);
        // Register static file handler last as a catch-all
        httpd_register_uri_handler(server, &static_files_uri);
        return server;
    }

    ESP_LOGE(TAG, "Error starting web server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    if (server) {
        ESP_LOGI(TAG, "Stopping web server");
        httpd_stop(server);
    }
}

// --- Auto Redial Timer Callback ---
void auto_redial_timer_callback(void* arg)
{
    if (is_bluetooth_connected && auto_redial_enabled && current_wifi_mode == WIFI_MODE_STA) {
        uint32_t extra = 0;
        if (redial_random_delay_seconds > 0) {
            extra = rand() % (redial_random_delay_seconds + 1); // 0..random_delay
        }
        last_random_delay_used = extra;
        ESP_LOGI(TAG, "Auto Redial Timer: Sending redial command... (random extra delay: %lu)", extra);
        esp_hf_client_dial(NULL); // Use NULL for last number redial
        if (extra > 0) {
            vTaskDelay(pdMS_TO_TICKS(extra * 1000)); // Wait extra seconds before next period
        }
    } else {
        ESP_LOGD(TAG, "Auto Redial Timer: Conditions not met for redial (BT Connected: %d, Auto Enabled: %d, WiFi Mode: %d)",
                 is_bluetooth_connected, auto_redial_enabled, current_wifi_mode);
    }
}

// --- Function to update the auto redial timer state ---
static void update_auto_redial_timer(void) {
    if (auto_redial_enabled && is_bluetooth_connected && current_wifi_mode == WIFI_MODE_STA) {
        if (esp_timer_is_active(auto_redial_timer)) {
            ESP_ERROR_CHECK(esp_timer_stop(auto_redial_timer));
            ESP_LOGI(TAG, "Stopped existing auto redial timer.");
        }
        // Seed random if not already seeded
        static bool seeded = false;
        if (!seeded) {
            srand((unsigned int)time(NULL));
            seeded = true;
        }
        ESP_ERROR_CHECK(esp_timer_start_periodic(auto_redial_timer, redial_period_seconds * 1000 * 1000)); // Period in microseconds
        ESP_LOGI(TAG, "Started auto redial timer with period %lu seconds.", redial_period_seconds);
    } else {
        if (esp_timer_is_active(auto_redial_timer)) {
            ESP_ERROR_CHECK(esp_timer_stop(auto_redial_timer));
            ESP_LOGI(TAG, "Stopped auto redial timer.");
        } else {
            ESP_LOGI(TAG, "Auto redial timer not active or conditions not met.");
        }
    }
}

// --- LED Morse Code Functions ---
static void init_led_gpio(void)
{
    gpio_set_direction(BUILTIN_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUILTIN_LED_PIN, 0); // LED off initially
    ESP_LOGI(TAG, "LED GPIO%d initialized for morse code", BUILTIN_LED_PIN);
}

static void morse_dot(void)
{
    gpio_set_level(BUILTIN_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(MORSE_DOT_DURATION));
    gpio_set_level(BUILTIN_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_PAUSE));
}

static void morse_dash(void)
{
    gpio_set_level(BUILTIN_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(MORSE_DASH_DURATION));
    gpio_set_level(BUILTIN_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_PAUSE));
}

static void morse_digit(char digit)
{
    switch (digit) {
        case '0': // -----
            morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
            break;
        case '1': // .----
            morse_dot(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
            break;
        case '2': // ..---
            morse_dot(); morse_dot(); morse_dash(); morse_dash(); morse_dash();
            break;
        case '3': // ...--
            morse_dot(); morse_dot(); morse_dot(); morse_dash(); morse_dash();
            break;
        case '4': // ....-
            morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dash();
            break;
        case '5': // .....
            morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
            break;
        case '6': // -....
            morse_dash(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
            break;
        case '7': // --...
            morse_dash(); morse_dash(); morse_dot(); morse_dot(); morse_dot();
            break;
        case '8': // ---..
            morse_dash(); morse_dash(); morse_dash(); morse_dot(); morse_dot();
            break;
        case '9': // ----.
            morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dot();
            break;
        case '.': // .-.-.-
            morse_dot(); morse_dash(); morse_dot(); morse_dash(); morse_dot(); morse_dash();
            break;
        default:
            // Unknown character, skip
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(MORSE_CHAR_PAUSE));
}

static void morse_ip_address(const char* ip_addr)
{
    if (ip_addr == NULL || strlen(ip_addr) == 0) {
        ESP_LOGW(TAG, "No IP address to signal in morse code");
        return;
    }
    
    ESP_LOGI(TAG, "Signaling IP address in morse code: %s", ip_addr);
    
    for (int i = 0; ip_addr[i] != '\0'; i++) {
        morse_digit(ip_addr[i]);
    }
}

static void morse_code_led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Morse code LED task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for IP address to be available
        if (strlen(current_ip_address) > 0) {
            morse_ip_address(current_ip_address);
        } else {
            ESP_LOGD(TAG, "No IP address available for morse code");
        }
        
        // 5 second pause between readouts
        vTaskDelay(pdMS_TO_TICKS(MORSE_IP_READOUT_PAUSE));
    }
}

static void signal_ip_change(void)
{
    ESP_LOGI(TAG, "IP address change signaled for morse code update");
}

// --- SPIFFS Initialization ---
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = WEB_MOUNT_POINT,
      .partition_label = NULL, // Use default partition label
      .max_files = 5,   // Max number of files that can be open at the same time
      .format_if_mount_failed = true // Format if mount fails
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ret;
}

// --- Main Application Entry Point ---
void app_main(void)
{
    esp_err_t ret;

    // --- Factory Reset Pin Check ---
    gpio_set_direction(FACTORY_RESET_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(FACTORY_RESET_PIN, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50)); // Allow pin to settle

    if (gpio_get_level(FACTORY_RESET_PIN) == 0) { // Pin pulled low
        ESP_LOGW(TAG, "FACTORY RESET PIN (GPIO%d) DETECTED LOW! Erasing NVS and SPIFFS...", FACTORY_RESET_PIN);
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "NVS erased successfully.");
        }
        
        // Also unmount and format SPIFFS
        esp_vfs_spiffs_unregister(NULL); // Unregister any existing mount
        ret = esp_spiffs_format(NULL); // Format default SPIFFS partition
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS format failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "SPIFFS formatted successfully. Performing factory reset.");
        }
        // After erase/format, re-initialize NVS and SPIFFS will be formatted on next mount if needed
    } else {
        ESP_LOGI(TAG, "FACTORY RESET PIN (GPIO%d) is HIGH. Proceeding with normal boot.", FACTORY_RESET_PIN);
    }

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // Should not happen if erase above worked
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS
    ESP_ERROR_CHECK(init_spiffs());

    // Initialize TCP/IP stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register Wi-Fi event handler
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Try to load Wi-Fi credentials from NVS
    char stored_ssid[32];
    char stored_password[64];
    if (load_wifi_credentials_from_nvs(stored_ssid, stored_password, sizeof(stored_ssid), sizeof(stored_password))) {
        ESP_LOGI(TAG, "Found stored Wi-Fi credentials. Starting in STA mode.");
        start_wifi_sta(stored_ssid, stored_password);
    } else {
        ESP_LOGI(TAG, "No stored Wi-Fi credentials. Starting in AP mode for configuration.");
        start_wifi_ap();
    }

    // Initialize Bluetooth controller and host
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE)); // Release BLE memory if not used
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT); // Enable Classic Bluetooth
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // Register GAP callback
    esp_bt_gap_register_callback(esp_bt_gap_cb);

    // Set Security Parameters for Secure Simple Pairing
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE; // No Input, No Output
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    /*
     * Set PIN type and code for legacy pairing
     * This is fallback for older devices, SSP will be used for modern phones
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    const char *pin_str = "1234";
    memcpy(pin_code, pin_str, 4);
    esp_bt_gap_set_pin(pin_type, 4, pin_code);
    
    // Set Class of Device to identify as Audio Headset device
    // CoD: Service=Audio(0x20) + Major=Audio/Video(0x04) + Minor=Headset(0x04)
    // Set Class of Device to identify as an Audio Headset device
    esp_bt_cod_t cod;
    cod.major = 0x04;     // Major Device Class: Audio/Video
    cod.minor = 0x04;     // Minor Device Class: Headset
    cod.service = 0x20;   // Service Class: Audio
    
    esp_err_t ret_cod = esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD);
    if (ret_cod == ESP_OK) {
        ESP_LOGI(TAG, "Successfully set Class of Device for Audio Headset");
    } else {
        ESP_LOGW(TAG, "Failed to set Class of Device: %s", esp_err_to_name(ret_cod));
    }

    // Set discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    // Set Bluetooth device name
    const char *device_name = "RemoteHead";
    esp_bt_dev_set_device_name(device_name);

    // Initialize HFP client
    ret = esp_hf_client_init();
    if (ret) {
        ESP_LOGE(TAG, "%s initialize HFP client failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_hf_client_register_callback(esp_hf_client_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s register HFP client callback failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // Load auto redial settings from NVS
    load_auto_redial_settings_from_nvs();

    // Create the auto redial timer (but don't start it yet, update_auto_redial_timer will handle it)
    const esp_timer_create_args_t auto_redial_timer_args = {
            .callback = &auto_redial_timer_callback,
            .name = "auto_redial_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&auto_redial_timer_args, &auto_redial_timer));

    // Initial update of the timer state based on loaded settings and current connection status
    update_auto_redial_timer();

    // Initialize LED GPIO for morse code
    init_led_gpio();

    // Create morse code LED task on CPU core 1
    xTaskCreatePinnedToCore(
        morse_code_led_task,
        "morse_led_task",
        2048,
        NULL,
        1,  // Priority
        &morse_code_task_handle,
        1   // CPU core 1
    );

    ESP_LOGI(TAG, "ESP32 HFP Headset Emulator with API initialized.");
}
