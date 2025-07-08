# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_project)

# Explicitly set the partition table file
set(CONFIG_PARTITION_TABLE_FILENAME "partitions.csv")
