cmake_minimum_required(VERSION 3.8)

add_library(util STATIC
			"interger.cpp" "integer.h"
            "logger.cpp" "logger.h"
			"using_integer.inl"
            "utility.cpp" "utility.h")

target_include_directories(util PUBLIC "${CMAKE_SOURCE_DIR}/src")
