# Test configuration for main.c unit tests
cmake_minimum_required(VERSION 3.16)

# Set this build as a test build
set(TESTING 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Only include essential components for testing
set(COMPONENTS "unity" "main_test" "esp_common" "freertos" "log" "newlib" "cjson" "nvs_flash" "esp_wifi")

project(remotehead_tests)