cmake_minimum_required(VERSION 3.8)

add_library(win32util STATIC
            "error.cpp" "error.h"
            "heap.cpp" "heap.h"
            "strings.cpp" "strings.h"
            "win32util.cpp" "win32util.h")

target_include_directories(win32util PUBLIC "${CMAKE_SOURCE_DIR}/src")
