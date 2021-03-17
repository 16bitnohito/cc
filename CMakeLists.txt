# CMakeList.txt : 最上位の CMake プロジェクト ファイル。グローバル構成を行います
# また、サブプロジェクトをここに含めます。
#
cmake_minimum_required(VERSION 3.8)

project("cc")

set(project_default_build_type "Debug")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "${project_default_build_type}")
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

# 共通設定
if (MSVC)
    string(REGEX REPLACE "/W[0-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(/permissive- /Zc:__cplusplus /Zc:throwingNew /Zc:referenceBinding /utf-8)
    add_compile_options(/w44189 /w44388 /w44800)
    # add_compile_options(/wd4061 /wd4514 /wd4711 /wd4820)
    add_compile_options($<$<CONFIG:Debug>:>
                        $<$<CONFIG:Release>:/Gy>
                        $<$<CONFIG:MinSizeRel>:/Gy>
                        $<$<CONFIG:RelWithDebgInfo>:/Gy>)
else ()
    # 手付かず
    add_compile_options(-Wall -Wextra -pedantic)
endif ()

if (WIN32)
    # Unicode版、内部文字列は WCHAR
    add_compile_definitions (_UNICODE UNICODE)
    # Unicode版、内部文字列の UTF-8 (charで扱う)に変換する。
    ## add_compile_definitions (UNICODE _UNICODE CONSOLEIO_PLATFORM_STRING_TO_UTF8)
    # Unicode版、内部文字列の UTF-8 (char8_tで扱う)に変換する。
    ## add_compile_definitions (UNICODE _UNICODE CONSOLEIO_PLATFORM_STRING_TO_UTF8 CONSOLEIO_USE_CHAR8_T_FOR_UTF8)

    # ANSI版、内部文字列は ANSI、何も定義しない。UNICODEか _UNICODEが無ければ /D _MBCSが自動で付くようだ。
    ## add_compile_definitions ()
    # ANSI版、内部文字列の UTF-8 (charで扱う)に変換する。
    ## add_compile_definitions (CONSOLEIO_PLATFORM_STRING_TO_UTF8)
    # ANSI版、内部文字列の UTF-8 (char8_tで扱う)に変換する。
    ## add_compile_definitions (CONSOLEIO_PLATFORM_STRING_TO_UTF8 CONSOLEIO_USE_CHAR8_T_FOR_UTF8)

    # ANSI(UTF-8)版、内部文字列は ANSI(UTF-8) (charで扱う)
    ## add_compile_definitions (WIN32UTIL_UTF8_CODE_PAGE)
    # ANSI(UTF-8)版、内部文字列は ANSI(UTF-8) (char8_tで扱う)
    ## add_compile_definitions (WIN32UTIL_UTF8_CODE_PAGE WIN32UTIL_USE_CHAR8_T_FOR_UTF8_CODE_PAGE CONSOLEIO_USE_CHAR8_T_FOR_UTF8)

    add_compile_definitions(_CONSOLE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>
                            $<$<CONFIG:Release>:NDEBUG>
                            $<$<CONFIG:MinSizeRel>:NDEBUG>
                            $<$<CONFIG:RelWithDebgInfo>:NDEBUG>)

    if (NOT MSVC)
        add_compile_options(-municode)
    endif ()
endif ()

# サブプロジェクトを含めます。
add_subdirectory("src")
