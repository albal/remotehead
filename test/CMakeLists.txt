# Test configuration for main.c unit tests
cmake_minimum_required(VERSION 3.16)

# Set this build as a test build
set(TESTING 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Exclude the main component to avoid duplicate app_main symbols
set(EXCLUDE_COMPONENTS "main")
set(COMPONENTS "unity" "main_test")

project(remotehead_tests)