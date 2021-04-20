cmake_minimum_required(VERSION 3.8)

add_subdirectory("preprocessor")
if (WIN32)
add_subdirectory("win32util")
endif()
