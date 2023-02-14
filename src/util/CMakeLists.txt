cmake_minimum_required(VERSION 3.8)

add_library(util STATIC
            "logger.cpp" "logger.h"
            "utility.cpp" "utility.h")

target_include_directories(util PUBLIC "${CMAKE_SOURCE_DIR}/src")
