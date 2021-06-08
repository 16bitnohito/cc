#ifndef CC_WIN32UTIL_STRINGS_H_
#define CC_WIN32UTIL_STRINGS_H_

#include <array>
#include <stdexcept>
#include <string>
#include <variant>
#include <Windows.h>
#include <win32util/error.h>
#include <win32util/heap.h>

namespace lib::win32util {

#if defined(UNICODE) && defined(WIN32UTIL_UTF8_CODE_PAGE)
#   error Invalid string setting.
#endif

#if defined(UNICODE)
// UNICODE
#   if defined(WIN32UTIL_CHAR8_T_CHAR8_T_FOR_UTF8_CODE_PAGE)
#       error Invalid UTF-8 codepage setting.
#   endif
#elif defined(WIN32UTIL_UTF8_CODE_PAGE)
// ANSI(UTF-8)
#else
// ANSI
#   if defined(WIN32UTIL_CHAR8_T_CHAR8_T_FOR_UTF8_CODE_PAGE)
#       error Invalid UTF-8 codepage setting.
#   endif
#endif

#if defined(WIN32UTIL_STRICT_CHAR_TYPE)
enum class MultiByteChar : char {};
enum class Utf8Char : char8_t {};
#else
#if defined(WIN32UTIL_CHAR____CHAR____FOR_UTF8_CODE_PAGE)
using MultiByteChar = char;
using Utf8Char = char;
#elif defined(WIN32UTIL_CHAR____CHAR8_T_FOR_UTF8_CODE_PAGE)
using MultiByteChar = char;
using Utf8Char = char8_t;
#elif defined(WIN32UTIL_CHAR8_T_CHAR____FOR_UTF8_CODE_PAGE)
#if !defined(WIN32UTIL_UTF8_CODE_PAGE)
#error ANSI(UTF-8) Required
#endif
using MultiByteChar = char8_t;
using Utf8Char = char;
#elif defined(WIN32UTIL_CHAR8_T_CHAR8_T_FOR_UTF8_CODE_PAGE)
#if !defined(WIN32UTIL_UTF8_CODE_PAGE)
#error ANSI(UTF-8) Required
#endif
using MultiByteChar = char8_t;
using Utf8Char = char8_t;
#else
using MultiByteChar = char;
using Utf8Char = char;
#endif
#endif

using WideChar = wchar_t;

/**
 */
using MultiByteString = std::basic_string<MultiByteChar>;
using MultiByteStringView = std::basic_string_view<MultiByteChar>;

using Utf8String = std::basic_string<Utf8Char>;
using Utf8StringView = std::basic_string_view<Utf8Char>;

using WideString = std::basic_string<WideChar>;
using WideStringView = std::basic_string_view<WideChar>;

/**
 */
#ifdef UNICODE
using Win32Char = WideChar;
#else   // !UNICODE
#if defined(WIN32UTIL_UTF8_CODE_PAGE) && defined(WIN32UTIL_USE_CHAR8_T_FOR_UTF8_CODE_PAGE)
using Win32Char = Utf8Char;
#else
using Win32Char = MultiByteChar;
#endif
#endif  // UNICODE

using Win32String = std::basic_string<Win32Char>;
using Win32StringView = std::basic_string_view<Win32Char>;

/**
 */
#ifdef UNICODE
#define WIN32TEXT_(l)   L ## l
#else   // !UNICODE
#if defined(WIN32UTIL_UTF8_CODE_PAGE) && defined(WIN32UTIL_USE_CHAR8_T_FOR_UTF8_CODE_PAGE)
#define WIN32TEXT_(l)   u8 ## l
#else
#define WIN32TEXT_(l)   l
#endif 
#endif  // UNICODE

/**
 */
#define WIN32TEXT(l)   WIN32TEXT_(l)

/**
 *
 */
template <class CharT>
struct Win32StringTraits;

template <>
struct Win32StringTraits<wchar_t> {
    using CharType = wchar_t;

    static constexpr wchar_t* as_native(CharType* p) noexcept {
        return p;
    }

    static constexpr const wchar_t* as_native(const CharType* p) noexcept {
        return p;
    }

    static constexpr CharType* as_win32(wchar_t* p) noexcept {
        return p;
    }

    static constexpr const CharType* as_win32(const wchar_t* p) noexcept {
        return p;
    }
};

#if defined(WIN32UTIL_STRICT_CHAR_TYPE)
template <>
struct Win32StringTraits<MultiByteChar> {
    using CharType = MultiByteChar;

    static char* as_native(CharType* p) noexcept {
        return reinterpret_cast<char*>(p);
    }

    static const char* as_native(const CharType* p) noexcept {
        return reinterpret_cast<const char*>(p);
    }

    static CharType* as_win32(char* p) noexcept {
        return reinterpret_cast<CharType*>(p);
    }

    static const CharType* as_win32(const char* p) noexcept {
        return reinterpret_cast<const CharType*>(p);
    }

    static Utf8Char* as_u8(CharType* p) noexcept {
        return reinterpret_cast<Utf8Char*>(p);
    }

    static const Utf8Char* as_u8(const CharType* p) noexcept {
        return reinterpret_cast<const Utf8Char*>(p);
    }
};

template <>
struct Win32StringTraits<Utf8Char> {
    using CharType = Utf8Char;

    static char* as_native(CharType* p) noexcept {
        return reinterpret_cast<char*>(p);
    }

    static const char* as_native(const CharType* p) noexcept {
        return reinterpret_cast<const char*>(p);
    }

    static CharType* as_win32(char* p) noexcept {
        return reinterpret_cast<CharType*>(p);
    }

    static const CharType* as_win32(const char* p) noexcept {
        return reinterpret_cast<const CharType*>(p);
    }
};
#else
template <>
struct Win32StringTraits<char> {
    using CharType = char;

    static constexpr char* as_native(CharType* p) noexcept {
        return p;
    }

    static constexpr const char* as_native(const CharType* p) noexcept {
        return p;
    }

    static constexpr CharType* as_win32(char* p) noexcept {
        return p;
    }

    static constexpr const CharType* as_win32(const char* p) noexcept {
        return p;
    }

    static Utf8Char* as_u8(CharType* p) noexcept {
        return reinterpret_cast<Utf8Char*>(p);
    }

    static const Utf8Char* as_u8(const CharType* p) noexcept {
        return reinterpret_cast<const Utf8Char*>(p);
    }
};

template <>
struct Win32StringTraits<char8_t> {
    using CharType = char8_t;

    static char* as_native(CharType* p) noexcept {
        return reinterpret_cast<char*>(p);
    }

    static const char* as_native(const CharType* p) noexcept {
        return reinterpret_cast<const char*>(p);
    }

    static CharType* as_win32(char* p) noexcept {
        return reinterpret_cast<char8_t*>(p);
    }

    static const CharType* as_win32(const char* p) noexcept {
        return reinterpret_cast<const char8_t*>(p);
    }
};
#endif

/**
 */
template <class CharT>
auto as_native(CharT* p) noexcept {
    return Win32StringTraits<CharT>::as_native(p);
}

/**
 */
template <class CharT>
auto as_native(const CharT* p) noexcept {
    return Win32StringTraits<CharT>::as_native(p);
}

/**
 */
template <class CharT>
constexpr
auto as_win32(CharT p) noexcept {
    return Win32StringTraits<Win32Char>::as_win32(p);
}

/**
 */
template <class CharT>
auto as_win32(CharT* p) noexcept {
    return Win32StringTraits<Win32Char>::as_win32(p);
}

/**
 */
template <class CharT>
auto as_win32(const CharT* p) noexcept {
    return Win32StringTraits<Win32Char>::as_win32(p);
}

/**
 */
template <class CharT>
auto as_u8(CharT* p) noexcept {
    return Win32StringTraits<CharT>::as_u8(p);
}

/**
 */
template <class CharT>
auto as_u8(const CharT* p) noexcept {
    return Win32StringTraits<CharT>::as_u8(p);
}


namespace detail {

template <class CharT>
WideString mbs_to_wcs_impl(std::basic_string_view<CharT> from, UINT from_cp) {
    if (std::ssize(from) > (std::numeric_limits<int>::max)()) {
        throw std::runtime_error(__func__);
    }
    if (from.empty()) {
        return WideString();
    }

    DWORD flags = 0;
    //constexpr DWORD flags = MB_PRECOMPOSED(default) or MB_COMPOSED | MB_USEGLYPHCHARS | MB_ERR_INVALID_CHARS;

    //  元のワイド文字列の長さとして、終端文字までを示す -1を渡さずに、length()を
    //  渡しているので、needsには終端文字の数が含まれないことに注意。
    auto needs = MultiByteToWideChar(
            from_cp, flags,
            as_native(from.data()), static_cast<int>(from.length()),
            nullptr, 0);
    if (needs == 0) {
        raise_win32_error("MultiByteToWideChar");
    }

    WideString result(static_cast<std::make_unsigned_t<decltype(needs)>>(needs), WIN32TEXT('\0'));
    auto converted = MultiByteToWideChar(
            from_cp, flags,
            as_native(from.data()), static_cast<int>(from.length()),
            as_native(result.data()), static_cast<int>(result.length()));
    if (converted == 0) {
        raise_win32_error("MultiByteToWideChar");
    }

    return result;
}

template <class CharT, class CharTraits = std::char_traits<CharT>>
std::basic_string<CharT, CharTraits> wcs_to_mbs_impl(WideStringView from, DWORD to_cp) {
    using StringType = std::basic_string<CharT, CharTraits>;

    if (std::ssize(from) > (std::numeric_limits<int>::max)()) {
        throw std::runtime_error(__func__);
    }
    if (from.empty()) {
        return StringType();
    }

    constexpr unsigned long flags = 0;  // or WC_ERR_INVALID_CHARS

    //  元のワイド文字列の長さとして、終端文字までを示す -1を渡さずに、length()を
    //  渡しているので、needsには終端文字の数が含まれないことに注意。
    auto needs = WideCharToMultiByte(
            to_cp, flags,
            as_native(from.data()), static_cast<int>(from.length()),
            nullptr, 0,
            nullptr, nullptr);
    if (needs == 0) {
        raise_win32_error("WideCharToMultiByte");
    }

    StringType result(static_cast<std::make_unsigned_t<decltype(needs)>>(needs), CharT());
    auto converted = WideCharToMultiByte(
            to_cp, flags,
            as_native(from.data()), static_cast<int>(from.length()),
            reinterpret_cast<char*>(result.data()), static_cast<int>(result.length()),
            nullptr, nullptr);
    if (converted == 0) {
        raise_win32_error("WideCharToMultiByte");
    }

    return result;
}

}   // namespace detial

/**
 */
WideString mbs_to_wcs(MultiByteStringView from, DWORD from_cp = CP_ACP);

/**
 */
Utf8String mbs_to_u8s(MultiByteStringView from, DWORD from_cp = CP_ACP);
Utf8String mbs_to_u8s(MultiByteString&& from, DWORD from_cp = CP_ACP);
Utf8String mbs_to_u8s(const MultiByteChar* from, DWORD from_cp = CP_ACP);

/**
 */
MultiByteString wcs_to_mbs(WideStringView from, DWORD to_cp = CP_ACP);
Utf8String wcs_to_u8s(WideStringView from);

/**
 * WIN32UTIL_UTF8_CODE_PAGEが定義され、且つ、UTF-8で動作している場合:
 *  fromは UTF-8の char又は char8_t、戻り値も UTF-8の char又は char8_tです。
 * それが定義されていない場合:
 *  fromは UTF-8の charr又は char8_t、戻り値は ACPの charです。
 */
MultiByteString u8s_to_mbs(Utf8StringView from);
MultiByteString u8s_to_mbs(Utf8String&& from);
MultiByteString u8s_to_mbs(const Utf8Char* from);

/**
 */
WideString u8s_to_wcs(Utf8StringView from);

/**
 */
inline
MultiByteString win32s_to_mbs(Win32StringView s) {
#if defined(UNICODE)
    return wcs_to_mbs(s);
#elif defined(WIN32UTIL_UTF8_CODE_PAGE)
    return u8s_to_mbs(s);
#else
    return s;
#endif
}

/**
 */
inline
Utf8String win32s_to_u8s(Win32StringView s) {
#if defined(UNICODE)
    return wcs_to_u8s(s);
#elif defined(WIN32UTIL_UTF8_CODE_PAGE)
    return s;
#else
    return mbs_to_u8s(s);
#endif
}

inline
Utf8String win32s_to_u8s(Win32String&& s) {
#if defined(UNICODE)
    return wcs_to_u8s(s);
#elif defined(WIN32UTIL_UTF8_CODE_PAGE)
    return std::move(s);
#else
    return mbs_to_u8s(s);
#endif
}

#if defined(WIN32UTIL_UTF8_CODE_PAGE)
/**
 */
inline
Utf8String& win32s_to_u8s(Win32String& s) {
    return s;
}

/**
 */
inline
const Utf8String& win32s_to_u8s(const Win32String& s) {
    return s;
}

/**
 */
inline
Utf8String win32s_to_u8s(Win32String&& s) {
    return std::move(s);
}
#endif

/**
 * 
 */
inline
Win32String u8s_to_win32s(Utf8StringView s) {
#if defined(UNICODE)
    return u8s_to_wcs(s);
#elif defined(WIN32UTIL_UTF8_CODE_PAGE)
    return s;
#else
    return u8s_to_mbs(s);
#endif
}

#if defined(WIN32UTIL_UTF8_CODE_PAGE)
/**
 * 
 */
inline
MultiByteString& u8s_to_win32s(Utf8String& s) {
    return s;
}

/**
 * 
 */
inline
const MultiByteString& u8s_to_win32s(const Utf8String& s) {
    return s;
}

/**
 * 
 */
inline
Win32String u8s_to_win32s(Utf8String&& s) {
    return std::move(s);
}
#endif

/**
 * LocalFreeする必要の有る文字列
 */
class LocalHeapString {
public:
    explicit LocalHeapString() noexcept
        : message_()
        , length_() {
    }

    explicit LocalHeapString(LocalHandle&& message, DWORD length) noexcept
        : message_(std::move(message))
        , length_(length) {
    }

    explicit LocalHeapString(const Win32Char* message, DWORD length) noexcept
        : message_(message)
        , length_(length) {
    }

    LocalHeapString(const LocalHeapString&) = delete;

    LocalHeapString(LocalHeapString&& other) noexcept
        : message_(std::move(other.message_))
        , length_(std::exchange(other.length_, 0LU)) {
    }

    ~LocalHeapString() {
        try {
            dispose();
        } catch (...) {
            // IGNORE
        }
    }

    LocalHeapString& operator=(const LocalHeapString&) = delete;

    LocalHeapString& operator=(LocalHeapString&& rhs) & noexcept {
        if (this != &rhs) {
            LocalHeapString temp = std::move(rhs);
            temp.swap(*this);
        }
        return *this;
    }

    LocalHeapString& operator=(LocalHeapString&&) && = delete;

    void swap(LocalHeapString& other) noexcept {
        std::swap(message_, other.message_);
        std::swap(length_, other.length_);
    }

    const Win32Char* c_str() const noexcept {
        if (std::holds_alternative<LocalHandle>(message_)) {
            const auto& m = std::get<LocalHandle>(message_);
            return m.handle() ? static_cast<const Win32Char*>(m.handle()) : WIN32TEXT("");
        } else {
            const auto& m = std::get<const Win32Char*>(message_);
            return m ? static_cast<const Win32Char*>(m) : WIN32TEXT("");
        }
    }

    DWORD length() const noexcept {
        return length_;
    }

    bool empty() const noexcept {
        return length() == 0;
    }

    void dispose();

private:
    std::variant<LocalHandle, const Win32Char*> message_;
    DWORD length_;
};


/**
 */
LocalHeapString last_error_string(DWORD code) noexcept;

namespace detail {

constexpr std::size_t kMaxFormatErrorMessageSize = 256;
extern std::array<Win32Char, kMaxFormatErrorMessageSize> format_error_message_buffer;

}

}   // namespace lib::win32util

#endif  // CC_WIN32UTIL_STRINGS_H_
