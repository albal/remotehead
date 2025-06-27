#include "config_utils.h"
#include <string.h>

int validate_phone_number(const char* number) {
    if (!number || strlen(number) == 0) {
        return 0; // Invalid
    }
    
    // Basic phone number validation - allow digits, +, -, spaces, parentheses
    for (const char* p = number; *p; p++) {
        if (!(*p >= '0' && *p <= '9') && 
            *p != '+' && *p != '-' && *p != ' ' && 
            *p != '(' && *p != ')') {
            return 0; // Invalid character
        }
    }
    
    return 1; // Valid
}

uint32_t clamp_redial_period(uint32_t period) {
    if (period < MIN_REDIAL_PERIOD) {
        return MIN_REDIAL_PERIOD;
    }
    if (period > MAX_REDIAL_PERIOD) {
        return MAX_REDIAL_PERIOD;
    }
    return period;
}

const char* wifi_mode_to_string(int mode) {
    switch (mode) {
        case 0: return "NULL";
        case 1: return "STA";
        case 2: return "AP";
        case 3: return "APSTA";
        default: return "Unknown";
    }
}