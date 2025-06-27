#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdint.h>
#include <stddef.h>

/**
 * Parse WiFi configuration from JSON string
 * @param json_str JSON string containing ssid and password
 * @param ssid Buffer to store SSID
 * @param password Buffer to store password
 * @param ssid_len Size of SSID buffer
 * @param password_len Size of password buffer
 * @return 0 on success, -1 on error
 */
int parse_wifi_config(const char* json_str, char* ssid, char* password, 
                      size_t ssid_len, size_t password_len);

/**
 * Parse auto redial configuration from JSON string
 * @param json_str JSON string containing enabled and period
 * @param enabled Pointer to store enabled flag
 * @param period Pointer to store period value
 * @return 0 on success, -1 on error
 */
int parse_auto_redial_config(const char* json_str, int* enabled, uint32_t* period);

/**
 * Create status response JSON string
 * @param bluetooth_connected Whether Bluetooth is connected
 * @param wifi_mode Current WiFi mode string
 * @param ip_address Current IP address
 * @param auto_redial_enabled Whether auto redial is enabled
 * @param redial_period Redial period in seconds
 * @return JSON string (caller must free), or NULL on error
 */
char* create_status_response(int bluetooth_connected, const char* wifi_mode, 
                            const char* ip_address, int auto_redial_enabled, 
                            uint32_t redial_period);

#endif // JSON_UTILS_H