#include "util/utility.h"

#include <cassert>
#include <iostream>
#include <system_error>
#if HOST_PLATFORM == PLATFORM_WINDOWS
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace std;
#if HOST_PLATFORM == PLATFORM_WINDOWS
using namespace lib::win32util;
#endif

namespace lib::util {

String get_current_dir() {
    String result;

#if HOST_PLATFORM == PLATFORM_WINDOWS
    DWORD needs = GetCurrentDirectory(0, nullptr);
    if (needs == 0) {
        raise_win32_error("GetCurrentDirectory");
    }

    Win32String temp(needs - 1, WIN32TEXT('\0'));
    DWORD written = GetCurrentDirectory(needs, as_native(temp.data()));
    if (written == 0) {
        raise_win32_error("GetCurrentDirectory");
    }

    result = win32s_to_u8s(move(temp));
#else
    size_t size = 256;
    char* p = nullptr;
    do {
        size *= 2;
        result.resize(size);
        p = getcwd(&result[0], size);
    } while (p == nullptr && errno == ERANGE);
#endif

    return result;
}

String trim_string(const String& s) {
    static const String blank = T_("\x09\x0b\x0c\x20");

    const auto l = s.find_first_not_of(blank);
    if (l == String::npos) {
        return String();
    }
    const auto r = s.find_last_not_of(blank);
    return s.substr(l, (r - l) + 1);
}

bool file_exists(const Path& path) {
#if HOST_PLATFORM == PLATFORM_WINDOWS
    struct _stat64 s;
    int e = _wstat64(path.c_str(), &s);

    return (e == 0);
#else
    struct stat s;
    int e = stat(path.c_str(), &s);

    return (e == 0);
#endif
}

void setup_console() {
#if HOST_PLATFORM == PLATFORM_WINDOWS
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        raise_generic_error("_setmode(stdin)", errno);
    }
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        raise_generic_error("_setmode(stdout)", errno);
    }
    if (_setmode(_fileno(stderr), _O_TEXT) == -1) {
        raise_generic_error("_setmode(stderr)", errno);
    }
#else
#endif

    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    cerr.tie(nullptr);
}

void raise_generic_error(const char* message, int error) {
    std::error_code ec(error, generic_category());
    throw std::system_error(ec, message);
}

void raise_generic_error(const char* message, std::errc error) {
    std::error_code ec = make_error_code(error);
    throw std::system_error(ec, message);
}

}   //  namespace lib::util