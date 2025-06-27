#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "lwip/ip_addr.h"
#include "driver/gpio.h" // For GPIO control
#include "cJSON.h" // For JSON parsing

#define TAG "HFP_REDIAL_API"

// --- Global Variables ---
bool is_bluetooth_connected = false; // Bluetooth HFP connection status
httpd_handle_t server = NULL; // HTTP server handle
char current_ip_address[IP4ADDR_STRLEN_MAX]; // To store current IP address
wifi_mode_t current_wifi_mode = WIFI_MODE_NULL; // To store current Wi-Fi mode

// Auto Redial Settings
bool auto_redial_enabled = false;
uint32_t redial_period_seconds = 60; // Default to 60 seconds

// Timer handle for automatic redial
esp_timer_handle_t auto_redial_timer;

// NVS Namespace and Keys
#define NVS_NAMESPACE "redial_config"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_AUTO_REDIAL_ENABLED "auto_en"
#define NVS_KEY_REDIAL_PERIOD "redial_period"

// AP Mode Configuration
#define AP_SSID "REMOTEHEAD"
#define AP_PASSWORD "" // No password
#define AP_MAX_CONN 4 // Max connections to AP

// GPIO Pin for Factory Reset (D13 on many ESP32 boards)
#define FACTORY_RESET_PIN GPIO_NUM_13

// --- Forward Declarations ---
static void esp_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t redial_get_handler(httpd_req_t *req);
static esp_err_t dial_get_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t configure_wifi_post_handler(httpd_req_t *req);
static esp_err_t set_auto_redial_post_handler(httpd_req_t *req); // New handler
static httpd_handle_t start_webserver(void);
static void stop_webserver(httpd_handle_t server);
static void start_wifi_ap(void);
static void start_wifi_sta(const char *ssid, const char *password);
static bool load_wifi_credentials_from_nvs(char *ssid, char *password, size_t ssid_len, size_t password_len);
static void save_wifi_credentials_to_nvs(const char *ssid, const char *password);
static bool load_auto_redial_settings_from_nvs(void); // New load function
static void save_auto_redial_settings_to_nvs(bool enabled, uint32_t period); // New save function
void auto_redial_timer_callback(void* arg); // Timer callback
static void update_auto_redial_timer(void); // Function to manage timer state

// --- HFP Client Callback ---
static void esp_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "HFP_CLIENT_EVT: %d", event);

    switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            if (param->conn_state.status == ESP_BT_STATUS_SUCCESS) {
                if (param->conn_state.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED) {
                    ESP_LOGI(TAG, "HFP Client Connected to phone!");
                    is_bluetooth_connected = true;
                    update_auto_redial_timer(); // Update timer state
                } else if (param->conn_state.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                    ESP_LOGI(TAG, "HFP Client Disconnected from phone!");
                    is_bluetooth_connected = false;
                    update_auto_redial_timer(); // Update timer state
                }
            } else {
                ESP_LOGE(TAG, "HFP Client Connection failed! Status: %d", param->conn_state.status);
            }
            break;
        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "HFP Audio State: %d", param->audio_state.state);
            break;
        case ESP_HF_CLIENT_BVRA_ENABLE_EVT:
            ESP_LOGI(TAG, "Voice recognition activated: %d", param->bvra_enable.state);
            break;
        case ESP_HF_CLIENT_CMD_CIND_STATUS_EVT:
        case ESP_HF_CLIENT_CMD_CIND_CURRENT_EVT:
            ESP_LOGI(TAG, "Call indicator status. Call setup: %d, Call: %d, Service: %d",
                     param->cind_status.call_setup_status,
                     param->cind_status.call_status,
                     param->cind_status.service_availability);
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
            ESP_LOGI(TAG, "ESP_BT_GAP_PIN_REQ_EVT min_len:%d", param->pin_req.min_len);
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
            esp_bt_gap_ssp_variant_reply(param->key_req.bda, ESP_BT_SSP_VARIANT_PASSKEY_ENTRY);
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

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded auto redial settings: Enabled=%s, Period=%lu seconds",
             auto_redial_enabled ? "true" : "false", redial_period_seconds);
    return true;
}

static void save_auto_redial_settings_to_nvs(bool enabled, uint32_t period) {
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

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS auto redial changes!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Saved auto redial settings: Enabled=%s, Period=%lu seconds",
             enabled ? "true" : "false", period);
}


// --- Wi-Fi Event Handler ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "Wi-Fi AP started. Connect to SSID: %s", AP_SSID);
            current_wifi_mode = WIFI_MODE_AP;
            strcpy(current_ip_address, "192.168.4.1"); // Default AP IP
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
            update_auto_redial_timer(); // Update timer state
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        ip4addr_ntoa_r(&event->ip_info.ip, current_ip_address, sizeof(current_ip_address));
        current_wifi_mode = WIFI_MODE_STA;
        if (server == NULL) {
            server = start_webserver(); // Start web server once IP is obtained
        }
        update_auto_redial_timer(); // Update timer state
    }
}

// --- Wi-Fi Initialization Functions ---
static void start_wifi_ap(void) {
    esp_netif_create_default_wifi_ap();

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
        esp_netif_destroy_default_wifi_ap(); // Destroy AP interface
    }

    esp_netif_create_default_wifi_sta(); // Create STA interface if not already present

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
    esp_hf_client_send_bldn(); // Send the redial command

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

    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (cJSON_IsString(ssid_json) && (ssid_json->valuestring != NULL) &&
        cJSON_IsString(password_json) && (password_json->valuestring != NULL)) {

        save_wifi_credentials_to_nvs(ssid_json->valuestring, password_json->valuestring);

        ESP_LOGI(TAG, "Switching to STA mode with SSID: %s", ssid_json->valuestring);
        stop_webserver(server); // Stop server before Wi-Fi mode change
        server = NULL; // Clear server handle
        ESP_ERROR_CHECK(esp_wifi_stop()); // Stop current Wi-Fi mode
        esp_netif_deinit(); // De-initialize netif for clean switch
        esp_netif_init(); // Re-initialize netif
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

    if (cJSON_IsBool(enabled_json) && cJSON_IsNumber(period_json)) {
        auto_redial_enabled = cJSON_IsTrue(enabled_json);
        redial_period_seconds = (uint32_t)cJSON_GetNumberValue(period_json);

        // Clamp period to valid range
        if (redial_period_seconds < 10) redial_period_seconds = 10;
        if (redial_period_seconds > 84600) redial_period_seconds = 84600;

        save_auto_redial_settings_to_nvs(auto_redial_enabled, redial_period_seconds);
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

static httpd_uri_t set_auto_redial_uri = { // New URI handler
    .uri       = "/set_auto_redial",
    .method    = HTTP_POST,
    .handler   = set_auto_redial_post_handler,
    .user_ctx  = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 5; // Increased to accommodate new handler

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &redial_uri);
        httpd_register_uri_handler(server, &dial_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &configure_wifi_uri);
        httpd_register_uri_handler(server, &set_auto_redial_uri); // Register new handler
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
        ESP_LOGI(TAG, "Auto Redial Timer: Sending redial command (AT+BLDN)...");
        esp_hf_client_send_bldn();
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


// --- Main Application Entry Point ---
void app_main(void)
{
    esp_err_t ret;

    // --- Factory Reset Pin Check ---
    gpio_set_direction(FACTORY_RESET_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(FACTORY_RESET_PIN, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50)); // Allow pin to settle

    if (gpio_get_level(FACTORY_RESET_PIN) == 0) { // Pin pulled low
        ESP_LOGW(TAG, "FACTORY RESET PIN (GPIO%d) DETECTED LOW! Erasing NVS...", FACTORY_RESET_PIN);
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "NVS erased successfully. Performing factory reset.");
        }
    } else {
        ESP_LOGI(TAG, "FACTORY RESET PIN (GPIO%d) is HIGH. Proceeding with normal boot.", FACTORY_RESET_PIN);
    }

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_MEM || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // Should not happen if erase above worked
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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

    // Set discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    // Set Bluetooth device name
    const char *device_name = "ESP32_Redial_API";
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

    ESP_LOGI(TAG, "ESP32 HFP Headset Emulator with API initialized.");
}
