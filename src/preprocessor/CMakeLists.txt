# CMakeList.txt : cpp の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#
cmake_minimum_required(VERSION 3.8)

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable(cpp
               "calculator.cpp" "calculator.hpp"
               "config.hpp"
               "diagnostics.cpp" "diagnostics.hpp"
               "main.cpp"
               "preprocessor.cpp" "preprocessor.hpp"
               "scanner.cpp" "scanner.hpp"
               "token.cpp" "token.hpp"
               "utility.cpp" "utility.hpp")
	
target_include_directories(cpp PRIVATE "../../src")
# target_link_libraries (cpp PRIVATE util win32util)

# TODO: テストを追加し、必要な場合は、ターゲットをインストールします。
