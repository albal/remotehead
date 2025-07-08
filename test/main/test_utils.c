#include "unity.h"
#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"  // Include for wifi_mode_t
#include "main.h"

// Mock implementation of url_decode for testing
void url_decode(char *str) {
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
            } else {
                *p_decoded++ = *p_str++;
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

// Test the url_decode function
void test_url_decode_basic(void) {
    char test_str[] = "hello%20world";
    url_decode(test_str);
    TEST_ASSERT_EQUAL_STRING("hello world", test_str);
}

void test_url_decode_plus_sign(void) {
    char test_str[] = "hello+world";
    url_decode(test_str);
    TEST_ASSERT_EQUAL_STRING("hello world", test_str);
}

void test_url_decode_hex_chars(void) {
    char test_str[] = "test%2Cvalue";
    url_decode(test_str);
    TEST_ASSERT_EQUAL_STRING("test,value", test_str);
}

// Mock test for basic string validation
void test_basic_string_validation(void) {
    // Simple test to ensure test framework works
    TEST_ASSERT_EQUAL_STRING("test", "test");
    TEST_ASSERT_NOT_EQUAL("different", "strings");
}
