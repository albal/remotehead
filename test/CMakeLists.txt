# Test configuration for main.c unit tests
cmake_minimum_required(VERSION 3.16)

# Set this build as a test build
set(TESTING 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(remotehead_tests)

# Enable Unity testing framework
set(COMPONENTS "unity" "main_test")