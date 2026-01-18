add_library(strings STATIC
            "strings.cpp" "strings.h")

target_include_directories(strings PUBLIC "${CMAKE_SOURCE_DIR}/src")
