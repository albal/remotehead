#include "include/simple_test.h"
#include "config_utils.h"

REGISTER_TEST(test_validate_phone_number_accepts_valid_numbers)
int test_validate_phone_number_accepts_valid_numbers(void) {
    TEST_ASSERT_TRUE(validate_phone_number("1234567890"));
    TEST_ASSERT_TRUE(validate_phone_number("+1-234-567-8900"));
    TEST_ASSERT_TRUE(validate_phone_number("(555) 123-4567"));
    TEST_ASSERT_TRUE(validate_phone_number("+44 20 7946 0958"));
    return 1;
}

REGISTER_TEST(test_validate_phone_number_rejects_invalid_numbers)
int test_validate_phone_number_rejects_invalid_numbers(void) {
    TEST_ASSERT_FALSE(validate_phone_number(""));
    TEST_ASSERT_FALSE(validate_phone_number(NULL));
    TEST_ASSERT_FALSE(validate_phone_number("abc123"));
    TEST_ASSERT_FALSE(validate_phone_number("123@456"));
    TEST_ASSERT_FALSE(validate_phone_number("123#456"));
    return 1;
}

REGISTER_TEST(test_clamp_redial_period_enforces_minimum)
int test_clamp_redial_period_enforces_minimum(void) {
    TEST_ASSERT_EQUAL_INT(MIN_REDIAL_PERIOD, clamp_redial_period(5));
    TEST_ASSERT_EQUAL_INT(MIN_REDIAL_PERIOD, clamp_redial_period(0));
    TEST_ASSERT_EQUAL_INT(MIN_REDIAL_PERIOD, clamp_redial_period(MIN_REDIAL_PERIOD - 1));
    return 1;
}

REGISTER_TEST(test_clamp_redial_period_enforces_maximum)
int test_clamp_redial_period_enforces_maximum(void) {
    TEST_ASSERT_EQUAL_INT(MAX_REDIAL_PERIOD, clamp_redial_period(MAX_REDIAL_PERIOD + 1));
    TEST_ASSERT_EQUAL_INT(MAX_REDIAL_PERIOD, clamp_redial_period(100000));
    return 1;
}

REGISTER_TEST(test_clamp_redial_period_preserves_valid_values)
int test_clamp_redial_period_preserves_valid_values(void) {
    TEST_ASSERT_EQUAL_INT(60, clamp_redial_period(60));
    TEST_ASSERT_EQUAL_INT(MIN_REDIAL_PERIOD, clamp_redial_period(MIN_REDIAL_PERIOD));
    TEST_ASSERT_EQUAL_INT(MAX_REDIAL_PERIOD, clamp_redial_period(MAX_REDIAL_PERIOD));
    return 1;
}

REGISTER_TEST(test_wifi_mode_to_string_returns_correct_values)
int test_wifi_mode_to_string_returns_correct_values(void) {
    TEST_ASSERT_EQUAL_STRING("NULL", wifi_mode_to_string(WIFI_MODE_NULL));
    TEST_ASSERT_EQUAL_STRING("STA", wifi_mode_to_string(WIFI_MODE_STA));
    TEST_ASSERT_EQUAL_STRING("AP", wifi_mode_to_string(WIFI_MODE_AP));
    TEST_ASSERT_EQUAL_STRING("APSTA", wifi_mode_to_string(WIFI_MODE_APSTA));
    TEST_ASSERT_EQUAL_STRING("Unknown", wifi_mode_to_string(999));
    return 1;
}