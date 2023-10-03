#ifndef CC_STRINGS_STRINGS_H_
#define CC_STRINGS_STRINGS_H_

#include <filesystem>
#include <string>

#include "config.h"

namespace lib::strings {

#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
enum class Utf8Char : char8_t {};
#else
using Utf8Char = char8_t;
#endif
using Utf8String = std::basic_string<Utf8Char>;
using Utf8StringView = std::basic_string_view<Utf8Char>;

}   // lib::strings

template<>
struct std::formatter<char8_t> : std::formatter<char> {
    template<class FormatContext>
    typename FormatContext::iterator format(const char8_t& c, FormatContext& ctx) const {
        return std::formatter<char>::format(static_cast<char>(c), ctx);
    }
};

template<>
struct std::formatter<char8_t*> : std::formatter<char*> {
    template<class FormatContext>
    typename FormatContext::iterator format(const char8_t* const& s, FormatContext& ctx) const {
        return std::formatter<char*>::format(reinterpret_cast<char*>(s), ctx);
    }
};

template<>
struct std::formatter<const char8_t*> : std::formatter<const char*> {
    template<class FormatContext>
    typename FormatContext::iterator format(const char8_t* const& s, FormatContext& ctx) const {
        return std::formatter<const char*>::format(reinterpret_cast<const char*>(s), ctx);
    }
};

template<std::size_t N>
struct std::formatter<char8_t[N]> : std::formatter<char[N]> {
    template<class FormatContext>
    typename FormatContext::iterator format(const char8_t(& a)[N], FormatContext& ctx) const {
        return std::formatter<char[N]>::format(reinterpret_cast<const char (&)[N]>(a), ctx);
    }
};

template<>
struct std::formatter<std::u8string> : std::formatter<std::string_view> {
    template<class FormatContext>
    typename FormatContext::iterator format(const std::u8string& s, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format({ reinterpret_cast<const char*>(s.data()), s.size() }, ctx);
    }
};

template<>
struct std::formatter<std::u8string_view> : std::formatter<std::string_view> {
    template<class FormatContext>
    typename FormatContext::iterator format(const std::u8string_view& s, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format({ reinterpret_cast<const char*>(s.data()), s.size() }, ctx);
    }
};


#if defined(CC_STRINGS_STRICT_CHAR_TYPE)
template<>
struct std::formatter<lib::strings::Utf8Char> : std::formatter<char> {
    template<class FormatContext>
    typename FormatContext::iterator format(const lib::strings::Utf8Char& c, FormatContext& ctx) const {
        return std::formatter<char>::format(static_cast<char>(c), ctx);
    }
};

template<>
struct std::formatter<lib::strings::Utf8Char*> : std::formatter<char*> {
    template<class FormatContext>
    typename FormatContext::iterator format(const lib::strings::Utf8Char* const& s, FormatContext& ctx) const {
        return std::formatter<char*>::format(reinterpret_cast<char*>(s), ctx);
    }
};

template<>
struct std::formatter<const lib::strings::Utf8Char*> : std::formatter<const char*> {
    template<class FormatContext>
    typename FormatContext::iterator format(const lib::strings::Utf8Char* const& s, FormatContext& ctx) const {
        return std::formatter<const char*>::format(reinterpret_cast<const char*>(s), ctx);
    }
};

template<std::size_t N>
struct std::formatter<lib::strings::Utf8Char[N]> : std::formatter<char[N]> {
    template<class FormatContext>
    typename FormatContext::iterator format(const lib::strings::Utf8Char(&a)[N], FormatContext& ctx) const {
        return std::formatter<char[N]>::format(reinterpret_cast<const char(&)[N]>(a), ctx);
    }
};

template<>
struct std::formatter<std::basic_string<lib::strings::Utf8Char>> : std::formatter<std::string_view> {
    template<class FormatContext>
    typename FormatContext::iterator format(const std::basic_string<lib::strings::Utf8Char>& s, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format({ reinterpret_cast<const char*>(s.c_str()), s.size() }, ctx);
    }
};

template<>
struct std::formatter<std::basic_string_view<lib::strings::Utf8Char>> : std::formatter<std::string_view> {
    template<class FormatContext>
    typename FormatContext::iterator format(const std::basic_string_view<lib::strings::Utf8Char>& s, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format({ reinterpret_cast<const char*>(s.data()), s.size() }, ctx);
    }
};

// こういう事をしないとならなくなるので、CC_STRINGS_STRICT_CHAR_TYPE関連は諦めるかもしれない。
#if defined(_MSC_VER)
template <>
struct std::_Is_character<lib::strings::Utf8Char> : true_type {};
#endif

#endif  // CC_STRINGS_STRICT_CHAR_TYPE

#endif  // CC_STRINGS_STRINGS_H_
