﻿# CMakeList.txt : 最上位の CMake プロジェクト ファイル。グローバル構成を行います
# また、サブプロジェクトをここに含めます。
#
cmake_minimum_required(VERSION 3.8)

project("cc")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

if (NOT CMAKE_VERBOSE_MAKEFILE)
    set(CMAKE_VERBOSE_MAKEFILE on)
endif ()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED on)
set(CMAKE_C_EXTENSIONS off)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

configure_file(
	"${PROJECT_SOURCE_DIR}/config.h.in"
	"${PROJECT_BINARY_DIR}/config.h")

# 共通設定
include_directories("${PROJECT_BINARY_DIR}")

if (MSVC)
    string(REGEX REPLACE "/W[0-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(
                        /wd4061     # switch 中の列挙子 '' ('' の) は case ラベルによって明示的にハンドルされません。
                        #/wd4245     # '' から '' に変換しました。signed/unsigned が一致しません。
                        #/wd4365     # '' から '' に変換しました。signed/unsigned が一致しません。
                        /wd4514     # 参照されていないインライン関数は削除されました。
                        /wd4625     # コピー コンストラクターは暗黙的に削除済みとして定義されました
                        /wd4626     # 代入演算子は暗黙的に削除済みとして定義されました
                        /wd4710     # インライン関数ではありません。
                        /wd4711     # 関数 '' に自動インライン展開を選択しました。
                        /wd4820     # '' バイトのパディングを データ メンバー '' の後に追加しました。
                        /wd5026     # 移動コンストラクターが暗黙的に削除済みとして定義されました
                        /wd5027     # 移動代入演算子が暗黙的に削除済みとして定義されました
                        /wd5045)    # spectre
    add_compile_options(/permissive- /Zc:__cplusplus /Zc:throwingNew /Zc:referenceBinding /utf-8)
    add_compile_options($<$<CONFIG:Debug>:>
                        $<$<CONFIG:Release>:/Gy>
                        $<$<CONFIG:MinSizeRel>:/Gy>
                        $<$<CONFIG:RelWithDebInfo>:/Gy>)
else ()
    # 手付かず
    add_compile_options(-Wall -Wextra -pedantic)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Weverything
                            -Wno-c++98-compat
                            -Wno-c++98-compat-pedantic
                            -Wno-exit-time-destructors
                            -Wno-switch-enum
                            -Wno-global-constructors)
    endif()
endif ()

# ターゲット
if (WIN32)
    # Unicode版
    add_compile_definitions (_UNICODE UNICODE)
    # ANSI(UTF-8)版、内部文字列は ANSI(UTF-8) (charで扱う)
    ## add_compile_definitions (WIN32UTIL_UTF8_CODE_PAGE)
    # ANSI(UTF-8)版、内部文字列は ANSI(UTF-8) (char8_tで扱う)
    ## add_compile_definitions (WIN32UTIL_UTF8_CODE_PAGE WIN32UTIL_USE_CHAR8_T_FOR_UTF8_CODE_PAGE)

	#add_compile_definitions(CC_STRINGS_STRICT_CHAR_TYPE)

    add_compile_definitions(_CONSOLE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>
                            $<$<CONFIG:Release>:>
                            $<$<CONFIG:MinSizeRel>:>
                            $<$<CONFIG:RelWithDebInfo>:>)

    if (NOT MSVC)
        add_compile_options(-municode)
        add_link_options(-municode)
    endif ()
endif ()

# サブプロジェクトを含めます。
add_subdirectory("src")
