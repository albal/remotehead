#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

#include <stdint.h>

// Constants from main.c
#define MIN_REDIAL_PERIOD 10
#define MAX_REDIAL_PERIOD 84600

// WiFi mode constants (matching ESP32 values)
#define WIFI_MODE_NULL    0
#define WIFI_MODE_STA     1  
#define WIFI_MODE_AP      2
#define WIFI_MODE_APSTA   3

/**
 * Validate a phone number string
 * @param number The phone number to validate
 * @return 1 if valid, 0 if invalid
 */
int validate_phone_number(const char* number);

/**
 * Clamp redial period to valid range
 * @param period The period in seconds
 * @return Clamped period value
 */
uint32_t clamp_redial_period(uint32_t period);

/**
 * Convert WiFi mode enum to string
 * @param mode WiFi mode enum value
 * @return String representation of mode
 */
const char* wifi_mode_to_string(int mode);

#endif // CONFIG_UTILS_H