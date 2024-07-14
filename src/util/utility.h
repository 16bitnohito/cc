#ifndef CC_UTIL_UTILITY_H_
#define CC_UTIL_UTILITY_H_

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "config.h"
#include "strings/strings.h"
#if HOST_PLATFORM == PLATFORM_WINDOWS
#include "win32util/win32util.h"
#endif

namespace lib::util {

#if HOST_PLATFORM == PLATFORM_WINDOWS
using Char = lib::strings::Utf8Char;
using PlatformChar = lib::win32util::Win32Char;
using PathChar = std::filesystem::path::value_type;
#else
using Char = char;
using PlatformChar = char;
using PathChar = char;
#endif

using String = std::basic_string<lib::strings::Utf8Char>;
using StringView = std::basic_string_view<lib::strings::Utf8Char>;
using PlatformString =  lib::win32util::Win32String;
using PlatformStringView = lib::win32util::Win32StringView;
using Path = std::filesystem::path;
//using Path = std::basic_string<PathChar>;

#if HOST_PLATFORM == PLATFORM_WINDOWS
#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
#define T__(l)  lib::util::as_internal(l)
#else
#define T__(l)  u8 ## l
//#define T__(l)  l
//#define T__(l)  L ## l
#endif
#else   // HOST_PLATFORM != PLATFORM_WINDOWS
#define T__(l)  l
#endif  // HOST_PLATFORM == PLATFORM_WINDOWS

#define T_(l)   T__(l)

/**
 */
[[nodiscard]] inline constexpr
Char as_internal(char c) noexcept {
    return static_cast<Char>(c);
}

/**
 */
[[nodiscard]] inline
const Char* as_internal(const char* s) noexcept {
    return reinterpret_cast<const Char*>(s);
}

/**
 */
template<std::size_t N>
[[nodiscard]] inline
const Char(& as_internal(const char(& a)[N]) noexcept)[N] {
    return reinterpret_cast<const Char(&)[N]>(a);
}

/**
 */
[[nodiscard]] inline
const char* as_narrow(const Char* c) noexcept {
    return reinterpret_cast<const char*>(c);
}

/**
 */
[[nodiscard]] inline
const std::string_view as_narrow(StringView sv) noexcept {
    return { as_narrow(sv.data()), sv.size() };
}

#if HOST_PLATFORM == PLATFORM_WINDOWS
[[nodiscard]] inline
String internal_string(const PlatformChar* s) {
    return lib::win32util::wcs_to_u8s(s);
}

[[nodiscard]] inline
String internal_string(PlatformStringView s) {
    return lib::win32util::wcs_to_u8s(s);
}

[[nodiscard]] inline
String internal_string(const Path& s) {
    return s.u8string();
}

[[nodiscard]] inline
PlatformString platform_string(const Char* s) {
    return lib::win32util::u8s_to_wcs(s);
}

[[nodiscard]] inline
PlatformString platform_string(StringView s) {
    return lib::win32util::u8s_to_wcs(s);
}

//[[nodiscard]] inline
//PlatformString platform_string(const String& s) {
//  return lib::win32util::u8s_to_wcs(s);
//}

[[nodiscard]] inline
Path path_string(StringView s) {
    return lib::win32util::as_native(lib::win32util::u8s_to_wcs(s).c_str());
    //return lib::win32util::u8s_to_wcs(s);
}

[[nodiscard]] inline
std::string source_string(StringView s) {
    return std::string(as_narrow(s.data()), s.size());
}

[[nodiscard]] inline
String internal_from_source(std::string_view s) {
    return String(reinterpret_cast<const Char*>(s.data()), s.length());
}

[[nodiscard]] inline
std::string source_from_internal(const String& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.length());
}
#else
[[nodiscard]] inline constexpr
const Char* internal_string(const PlatformChar* s) {
    return s;
}

[[nodiscard]] inline constexpr
const String& internal_string(const PlatformString& s) {
    return s;
}

//[[nodiscard]] inline constexpr
//String internal_string(PlatformStringView s) {
//  return String(s.data(), s.length());
//}

[[nodiscard]] inline constexpr
const PlatformChar* platform_string(const Char* s) {
    return s;
}

[[nodiscard]] inline constexpr
const PlatformString& platform_string(const String& s) {
    return s;
}

[[nodiscard]] inline
String internal_string(const Path& s) {
    return s.string();
}

[[nodiscard]] inline constexpr
const PathChar* path_string(const Char* s) {
    return s;
}

[[nodiscard]] inline constexpr
const Path& path_string(const String& s) {
    return s;
}

[[nodiscard]] inline constexpr
const char* source_string(const Char* s) {
    return s;
}

[[nodiscard]] inline constexpr
const std::string& source_string(const String& s) {
    return s;
}

[[nodiscard]] inline constexpr
const Char* internal_from_source(const char* s) {
    return s;
}

[[nodiscard]] inline constexpr
const String& internal_from_source(const std::string& s) {
    return s;
}

[[nodiscard]] inline constexpr
const char* source_from_internal(const Char* s) {
    return s;
}

[[nodiscard]] inline constexpr
const std::string& source_from_internal(const String& s) {
    return s;
}
#endif

String get_current_dir();

String trim_string(const String& s);

template <class T>
std::size_t size_in_bytes(const std::vector<T>& container) {
    return container.size() * sizeof(std::vector<T>::value_type);
}

bool file_exists(const Path& path);

struct Sorter {
    template <class Container>
    Sorter(Container& c) {
        using std::sort;
        sort(c.begin(), c.end());
    }

    template <class Container, class Pred>
    Sorter(Container& c, Pred pred) {
        using std::sort;
        sort(c.begin(), c.end(), pred);
    }
};

template <class Iter, class T, class Pred = std::less<T>>
Iter binary_find(Iter first, Iter last, const T& x, Pred pred = Pred()) {
    Iter it = std::lower_bound(first, last, x, pred);
    return ((it != last) && !pred(x, *it)) ? it : last;
}

/**
 */
constexpr
auto enum_ordinal(auto value) {
    return static_cast<std::underlying_type_t<decltype(value)>>(value);
}

template<std::integral I>
constexpr
I to_int(I v) {
    return v;
}

template<class E>
requires(std::is_enum_v<E>)
constexpr
auto to_int(E v) {
    return enum_ordinal(v);
}

/**
 */
void setup_console();

/**
 * generic_categoryの system_errorを送出します。
 * 
 * @param error ヘッダー system_errorの std::errc、或いは、ヘッダー cerrnoのエラー値や errnoを渡します。
 */
[[noreturn]]
void raise_generic_error(const char* message, int error);

[[noreturn]]
void raise_generic_error(const char* message, std::errc error);

// system_categoryの system_errorに関しては、処理系によって messageの扱いが
// 異なるので、そのまま取り扱わない事にしました。例えば VCは system_errorを
// Win32エラー値で送出しますが、他はそうではありません。そこで、このアプリでは
// Win32エラーは、win32util内でwin32_categoryを定義し、それをエラー値と共に
// lib::win32util::raise_win32_errorから送出しています。

std::string normalize_string(const std::string& s);


/**
 */
std::optional<String>  get_env_var(const Char* name);

}   // namespace lib::util

#endif  // CC_UTIL_UTILITY_H_
