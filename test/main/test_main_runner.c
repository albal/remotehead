#include "unity.h"
#include "esp_log.h"

// Forward declare the test runner function from main_test component
extern void run_all_tests(void);

void app_main(void)
{
    ESP_LOGI("TEST", "Starting ESP32 Remotehead Unit Tests");
    
    // Call the test runner from main_test component
    run_all_tests();
}