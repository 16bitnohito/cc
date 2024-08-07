#include "strings.h"

#include <limits>
#include <stdexcept>
#include <tchar.h>

using namespace std;

namespace lib::win32util {

WideString mbs_to_wcs(MultiByteStringView from, DWORD from_cp /*= CP_ACP*/) {
    return detail::mbs_to_wcs_impl(from, from_cp);
}

#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
WideString mbs_to_wcs(std::string_view from, DWORD from_cp /*= CP_ACP*/) {
    return detail::mbs_to_wcs_impl(from, from_cp);
}
#endif  // CC_STRINGS_STRICT_CHAR_TYPE

lib::strings::Utf8String mbs_to_u8s(MultiByteStringView from, DWORD from_cp /*= CP_ACP*/) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return from;
#else
    if (from_cp == CP_UTF8) {
        return lib::strings::Utf8String(reinterpret_cast<const lib::strings::Utf8Char*>(from.data()), from.length());
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}

lib::strings::Utf8String mbs_to_u8s(MultiByteString&& from, DWORD from_cp /*= CP_ACP*/) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return move(from);
#else
    if (from_cp == CP_UTF8) {
#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
        return as_u8(from.c_str());
#else
        return as_u8(from.data());
#endif
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}

lib::strings::Utf8String mbs_to_u8s(const MultiByteChar* from, DWORD from_cp /*= CP_ACP*/) {
    if (from == nullptr) {
        throw invalid_argument(__func__);
    }

#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return from;
#else
    if (from_cp == CP_UTF8) {
        return as_u8(from);
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}

#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
lib::strings::Utf8String mbs_to_u8s(std::string_view from, DWORD from_cp /*= CP_ACP*/) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return lib::strings::Utf8String(reinterpret_cast<const lib::strings::Utf8Char*>(from.data()), from.size());
#else
    if (from_cp == CP_UTF8) {
        return lib::strings::Utf8String(reinterpret_cast<const lib::strings::Utf8Char*>(from.data()), from.size());
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}

lib::strings::Utf8String mbs_to_u8s(std::string&& from, DWORD from_cp /*= CP_ACP*/) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return move(from);
#else
    if (from_cp == CP_UTF8) {
#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
        return as_u8(from.c_str());
#else
        return as_u8(from.data());
#endif
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}

lib::strings::Utf8String mbs_to_u8s(const char* from, DWORD from_cp /*= CP_ACP*/) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return lib::strings::Utf8String(reinterpret_cast<const lib::strings::Utf8Char*>(from));
#else
    if (from_cp == CP_UTF8) {
        return lib::strings::Utf8String(reinterpret_cast<const lib::strings::Utf8Char*>(from));
    } else {
        return wcs_to_u8s(mbs_to_wcs(from, from_cp));
    }
#endif
}
#endif  // CC_STRINGS_STRICT_CHAR_TYPE

MultiByteString wcs_to_mbs(WideStringView from, DWORD to_cp /*= CP_ACP*/) {
    return detail::wcs_to_mbs_impl<MultiByteChar>(from, to_cp);
}

std::string wcs_to_mbs(WideStringView from, std::string& to, DWORD to_cp /*= CP_ACP*/) {
    to = detail::wcs_to_mbs_impl<char>(from, to_cp);
    return to;
}

lib::strings::Utf8String wcs_to_u8s(WideStringView from) {
    return detail::wcs_to_mbs_impl<lib::strings::Utf8Char>(from, CP_UTF8);
}

std::string wcs_to_u8s(WideStringView from, std::string& to) {
    to = detail::wcs_to_mbs_impl<char>(from, CP_UTF8);
    return to;
}

MultiByteString u8s_to_mbs(lib::strings::Utf8StringView from) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return from;
#else
    return wcs_to_mbs(u8s_to_wcs(from));
#endif
}

MultiByteString u8s_to_mbs(lib::strings::Utf8String&& from) {
#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return move(from);
#else
    return wcs_to_mbs(u8s_to_wcs(from));
#endif
}

MultiByteString u8s_to_mbs(const lib::strings::Utf8Char* from) {
    if (from == nullptr) {
        throw invalid_argument(__func__);
    }

#if defined(WIN32UTIL_UTF8_CODE_PAGE)
    return from;
#else
    return wcs_to_mbs(u8s_to_wcs(from));
#endif
}

WideString u8s_to_wcs(lib::strings::Utf8StringView from) {
    return detail::mbs_to_wcs_impl(from, CP_UTF8);
}

WideString u8s_to_wcs(std::string_view from) {
    return detail::mbs_to_wcs_impl(from, CP_UTF8);
}


void LocalHeapString::dispose() {
    if (std::holds_alternative<LocalHandle>(message_)) {
        auto& m = std::get<LocalHandle>(message_);
        m.close();
    } else {
        auto& m = std::get<const Win32Char*>(message_);
        m = nullptr;
    }
    length_ = 0;
}

namespace detail {

std::array<Win32Char, kMaxFormatErrorMessageSize> format_error_message_buffer;

}

LocalHeapString last_error_string(DWORD error_code) noexcept {
    try {
        constexpr auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS;
        constexpr auto langid = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
        LPTSTR message = nullptr;
        const auto formatted = FormatMessage(
                flags,
                nullptr,
                error_code,
                langid,
                reinterpret_cast<LPTSTR>(&message),
                0,
                nullptr);
        if (formatted == 0) {
            const auto code = GetLastError();
            const auto ptr = as_native(detail::format_error_message_buffer.data());
            const auto written = _stprintf_s(
                    ptr, detail::format_error_message_buffer.size(),
                    WIN32TEXT("FormatMessage: 0x%lx"), code);
            return LocalHeapString(detail::format_error_message_buffer.data(), written);
        }

        return LocalHeapString(LocalHandle(message), formatted);
    } catch (...) {
        // error(__func__, MessageId::kUnknownException);
        // 起動時に最低限のメモリーを確保してそれを返すなどする方が良いかも。
        return LocalHeapString();
    }
}

WideString normalize_string(const WideString& s) {
    constexpr auto form = NormalizationC;
    int needs = NormalizeString(form, s.c_str(), -1, nullptr, 0);
    if (needs <= 0) {
        raise_win32_error("NormalizeString");
    }

    WideString result(needs - 1, L'\0');
    int written = NormalizeString(form, s.c_str(), -1, result.data(), needs);
    if (written <= 0) {
        raise_win32_error("NormalizeString");
    }

    result.resize(written - 1);
    return result;
}

}   // namespace win32util
