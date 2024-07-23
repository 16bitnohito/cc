# CMakeList.txt : cpp の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#
cmake_minimum_required(VERSION 3.8)

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable(cpp
               "calculator.cpp" "calculator.h"
               "diagnostics.cpp" "diagnostics.h"
               "main.cpp"
               "input.cpp" "input.h"
               "options.cpp" "options.h"
			   "pp_config.h"
               "preprocessor.cpp" "preprocessor.h"
               "scanner.cpp" "scanner.h"
			   "sourcefilestack.cpp" "sourcefilestack.h"
               "token.cpp" "token.h")

target_include_directories(cpp PRIVATE "../../src")
target_link_libraries(cpp PRIVATE strings)
target_link_libraries(cpp PRIVATE util)
if (WIN32)
target_link_libraries(cpp PRIVATE win32util)
endif()

# TODO: テストを追加し、必要な場合は、ターゲットをインストールします。
