#include "preprocessor/scanner.h"

#include <algorithm>
#include <cassert>

#include "util/utility.h"

using namespace lib::util;
using namespace std;

namespace {

enum {
    kOther = 0,
    kWsChar,
    kDigit,
    kLetter,
    kPunct,
};

constexpr int kInitial[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, kWsChar, kWsChar, kWsChar, kWsChar, kWsChar, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    kWsChar, kPunct, kPunct, 0, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct,
    kDigit, kDigit, kDigit, kDigit, kDigit, kDigit, kDigit, kDigit, kDigit, kDigit, kPunct, kPunct, kPunct, kPunct, kPunct, kPunct,
    0, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter,
    kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kPunct, 0, kPunct, kPunct, kPunct,
    0, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter,
    kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kLetter, kPunct, kPunct, kPunct, kPunct, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

constexpr inline
pp::Scanner::Char32 to_c32(std::underlying_type_t<pp::Scanner::Char32> c) {
    return static_cast<pp::Scanner::Char32>(c);
}

constexpr const uint8_t kUtf8Head[4] = {
    0x7f, 0x1f, 0x0f, 0x07,
};

constexpr
int c8_to_c32(const char* c8, size_t c8_bytes, pp::Scanner::Char32& c32) {
    if (c8_bytes <= 0) {
        return 0;
    }

    int c0 = c8[0] & 0xff;
    if (c0 <= 0x7f) {
        c32 = to_c32(c0);
        return 1;
    }

    int n = 0;
    uint8_t m = 0x80;
    for (int i = 0; i < 5; ++i) {
        if ((c0 & m) == 0) {
            break;
        }
        ++n;
        m >>= 1;
    }
    if (!(2 <= n && n <= 4)) {
        return 0;
    }
    if (n >= c8_bytes) {
        return 0;
    }

    uint32_t r = c0 & kUtf8Head[n - 1];
    for (int i = 1; i < n; ++i) {
        int c = c8[i];
        if ((c & 0xc0) != 0x80) {
            return 0;
        }

        r <<= 6;
        r |= (c & 0x3f);
    }

    c32 = to_c32(r);

    return n;
}

constexpr
int c32_to_c8(pp::Scanner::Char32 c32, char (&c8)[4]) {
    auto c = to_int(c32);
    if (c <= 0x7F) {
        c8[0] = static_cast<char>(c);
        return 1;
    } else if (c <= 0x07FF) {
        c8[0] = 0xc0 | ((c >> 6) & 0x1f);
        c8[1] = 0x80 | (c & 0x3f);
        return 2;
    } else if (c <= 0xFFFF) {
        c8[0] = 0xe0 | ((c >> 12) & 0x0f);
        c8[1] = 0x80 | ((c >> 6) & 0x3f);
        c8[2] = 0x80 | (c & 0x3f);
        return 3;
    } else if (c <= 0x10FFFF) {
        c8[0] = 0xf0 | ((c >> 18) & 0x07);
        c8[1] = 0x80 | ((c >> 12) & 0x3f);
        c8[2] = 0x80 | ((c >> 6) & 0x3f);
        c8[3] = 0x80 | (c & 0x3f);
        return 4;
    } else {
        throw runtime_error(__func__);
    }
}

int char_kind(pp::Scanner::Char32 c) {
    const auto i = to_int(c);
    if (i < 0 || size(kInitial) <= i) {
        return kOther;
    }

    auto k = kInitial[i];
    switch (k) {
    case kWsChar:
    case kDigit:
    case kLetter:
        return k;
    default:
        return kOther;
    }
}

inline
bool is_hex(pp::Scanner::Char32 c) {
    auto n = to_int(c);
    if (n > 255) {
        return false;
    }
    return isxdigit(n) != 0;
}

inline
bool is_oct(pp::Scanner::Char32 c) {
    return (to_c32('0') <= c && c <= to_c32('7'));
}

inline
bool is_digit(pp::Scanner::Char32 c) {
    return (to_c32('0') <= c && c <= to_c32('9'));
}

inline
bool is_nondigit(pp::Scanner::Char32 c) {
    auto n = to_int(c);
    if (n > 255) {
        return false;
    }
    return (c == to_c32('_') || isalpha(n));
}

inline
bool is_ws(pp::Scanner::Char32 c) {
    return c == to_c32('\x09') || c == to_c32('\x0b') || c == to_c32('\x0c') || c == to_c32('\x20');
}

inline
bool is_nl(pp::Scanner::Char32 c) {
    return c == to_c32('\x0a') || c == to_c32('\x0d');
}

// DerivedCoreProperties.txt, XID_Start
constexpr uint32_t xid_start_table[][2] = {
    { 0x0041, 0x005A },
    { 0x0061, 0x007A },
    { 0x00AA, 0x00AA },
    { 0x00B5, 0x00B5 },
    { 0x00BA, 0x00BA },
    { 0x00C0, 0x00D6 },
    { 0x00D8, 0x00F6 },
    { 0x00F8, 0x01BA },
    { 0x01BB, 0x01BB },
    { 0x01BC, 0x01BF },
    { 0x01C0, 0x01C3 },
    { 0x01C4, 0x0293 },
    { 0x0294, 0x0294 },
    { 0x0295, 0x02AF },
    { 0x02B0, 0x02C1 },
    { 0x02C6, 0x02D1 },
    { 0x02E0, 0x02E4 },
    { 0x02EC, 0x02EC },
    { 0x02EE, 0x02EE },
    { 0x0370, 0x0373 },
    { 0x0374, 0x0374 },
    { 0x0376, 0x0377 },
    { 0x037B, 0x037D },
    { 0x037F, 0x037F },
    { 0x0386, 0x0386 },
    { 0x0388, 0x038A },
    { 0x038C, 0x038C },
    { 0x038E, 0x03A1 },
    { 0x03A3, 0x03F5 },
    { 0x03F7, 0x0481 },
    { 0x048A, 0x052F },
    { 0x0531, 0x0556 },
    { 0x0559, 0x0559 },
    { 0x0560, 0x0588 },
    { 0x05D0, 0x05EA },
    { 0x05EF, 0x05F2 },
    { 0x0620, 0x063F },
    { 0x0640, 0x0640 },
    { 0x0641, 0x064A },
    { 0x066E, 0x066F },
    { 0x0671, 0x06D3 },
    { 0x06D5, 0x06D5 },
    { 0x06E5, 0x06E6 },
    { 0x06EE, 0x06EF },
    { 0x06FA, 0x06FC },
    { 0x06FF, 0x06FF },
    { 0x0710, 0x0710 },
    { 0x0712, 0x072F },
    { 0x074D, 0x07A5 },
    { 0x07B1, 0x07B1 },
    { 0x07CA, 0x07EA },
    { 0x07F4, 0x07F5 },
    { 0x07FA, 0x07FA },
    { 0x0800, 0x0815 },
    { 0x081A, 0x081A },
    { 0x0824, 0x0824 },
    { 0x0828, 0x0828 },
    { 0x0840, 0x0858 },
    { 0x0860, 0x086A },
    { 0x0870, 0x0887 },
    { 0x0889, 0x088E },
    { 0x08A0, 0x08C8 },
    { 0x08C9, 0x08C9 },
    { 0x0904, 0x0939 },
    { 0x093D, 0x093D },
    { 0x0950, 0x0950 },
    { 0x0958, 0x0961 },
    { 0x0971, 0x0971 },
    { 0x0972, 0x0980 },
    { 0x0985, 0x098C },
    { 0x098F, 0x0990 },
    { 0x0993, 0x09A8 },
    { 0x09AA, 0x09B0 },
    { 0x09B2, 0x09B2 },
    { 0x09B6, 0x09B9 },
    { 0x09BD, 0x09BD },
    { 0x09CE, 0x09CE },
    { 0x09DC, 0x09DD },
    { 0x09DF, 0x09E1 },
    { 0x09F0, 0x09F1 },
    { 0x09FC, 0x09FC },
    { 0x0A05, 0x0A0A },
    { 0x0A0F, 0x0A10 },
    { 0x0A13, 0x0A28 },
    { 0x0A2A, 0x0A30 },
    { 0x0A32, 0x0A33 },
    { 0x0A35, 0x0A36 },
    { 0x0A38, 0x0A39 },
    { 0x0A59, 0x0A5C },
    { 0x0A5E, 0x0A5E },
    { 0x0A72, 0x0A74 },
    { 0x0A85, 0x0A8D },
    { 0x0A8F, 0x0A91 },
    { 0x0A93, 0x0AA8 },
    { 0x0AAA, 0x0AB0 },
    { 0x0AB2, 0x0AB3 },
    { 0x0AB5, 0x0AB9 },
    { 0x0ABD, 0x0ABD },
    { 0x0AD0, 0x0AD0 },
    { 0x0AE0, 0x0AE1 },
    { 0x0AF9, 0x0AF9 },
    { 0x0B05, 0x0B0C },
    { 0x0B0F, 0x0B10 },
    { 0x0B13, 0x0B28 },
    { 0x0B2A, 0x0B30 },
    { 0x0B32, 0x0B33 },
    { 0x0B35, 0x0B39 },
    { 0x0B3D, 0x0B3D },
    { 0x0B5C, 0x0B5D },
    { 0x0B5F, 0x0B61 },
    { 0x0B71, 0x0B71 },
    { 0x0B83, 0x0B83 },
    { 0x0B85, 0x0B8A },
    { 0x0B8E, 0x0B90 },
    { 0x0B92, 0x0B95 },
    { 0x0B99, 0x0B9A },
    { 0x0B9C, 0x0B9C },
    { 0x0B9E, 0x0B9F },
    { 0x0BA3, 0x0BA4 },
    { 0x0BA8, 0x0BAA },
    { 0x0BAE, 0x0BB9 },
    { 0x0BD0, 0x0BD0 },
    { 0x0C05, 0x0C0C },
    { 0x0C0E, 0x0C10 },
    { 0x0C12, 0x0C28 },
    { 0x0C2A, 0x0C39 },
    { 0x0C3D, 0x0C3D },
    { 0x0C58, 0x0C5A },
    { 0x0C5D, 0x0C5D },
    { 0x0C60, 0x0C61 },
    { 0x0C80, 0x0C80 },
    { 0x0C85, 0x0C8C },
    { 0x0C8E, 0x0C90 },
    { 0x0C92, 0x0CA8 },
    { 0x0CAA, 0x0CB3 },
    { 0x0CB5, 0x0CB9 },
    { 0x0CBD, 0x0CBD },
    { 0x0CDD, 0x0CDE },
    { 0x0CE0, 0x0CE1 },
    { 0x0CF1, 0x0CF2 },
    { 0x0D04, 0x0D0C },
    { 0x0D0E, 0x0D10 },
    { 0x0D12, 0x0D3A },
    { 0x0D3D, 0x0D3D },
    { 0x0D4E, 0x0D4E },
    { 0x0D54, 0x0D56 },
    { 0x0D5F, 0x0D61 },
    { 0x0D7A, 0x0D7F },
    { 0x0D85, 0x0D96 },
    { 0x0D9A, 0x0DB1 },
    { 0x0DB3, 0x0DBB },
    { 0x0DBD, 0x0DBD },
    { 0x0DC0, 0x0DC6 },
    { 0x0E01, 0x0E30 },
    { 0x0E32, 0x0E32 },
    { 0x0E40, 0x0E45 },
    { 0x0E46, 0x0E46 },
    { 0x0E81, 0x0E82 },
    { 0x0E84, 0x0E84 },
    { 0x0E86, 0x0E8A },
    { 0x0E8C, 0x0EA3 },
    { 0x0EA5, 0x0EA5 },
    { 0x0EA7, 0x0EB0 },
    { 0x0EB2, 0x0EB2 },
    { 0x0EBD, 0x0EBD },
    { 0x0EC0, 0x0EC4 },
    { 0x0EC6, 0x0EC6 },
    { 0x0EDC, 0x0EDF },
    { 0x0F00, 0x0F00 },
    { 0x0F40, 0x0F47 },
    { 0x0F49, 0x0F6C },
    { 0x0F88, 0x0F8C },
    { 0x1000, 0x102A },
    { 0x103F, 0x103F },
    { 0x1050, 0x1055 },
    { 0x105A, 0x105D },
    { 0x1061, 0x1061 },
    { 0x1065, 0x1066 },
    { 0x106E, 0x1070 },
    { 0x1075, 0x1081 },
    { 0x108E, 0x108E },
    { 0x10A0, 0x10C5 },
    { 0x10C7, 0x10C7 },
    { 0x10CD, 0x10CD },
    { 0x10D0, 0x10FA },
    { 0x10FC, 0x10FC },
    { 0x10FD, 0x10FF },
    { 0x1100, 0x1248 },
    { 0x124A, 0x124D },
    { 0x1250, 0x1256 },
    { 0x1258, 0x1258 },
    { 0x125A, 0x125D },
    { 0x1260, 0x1288 },
    { 0x128A, 0x128D },
    { 0x1290, 0x12B0 },
    { 0x12B2, 0x12B5 },
    { 0x12B8, 0x12BE },
    { 0x12C0, 0x12C0 },
    { 0x12C2, 0x12C5 },
    { 0x12C8, 0x12D6 },
    { 0x12D8, 0x1310 },
    { 0x1312, 0x1315 },
    { 0x1318, 0x135A },
    { 0x1380, 0x138F },
    { 0x13A0, 0x13F5 },
    { 0x13F8, 0x13FD },
    { 0x1401, 0x166C },
    { 0x166F, 0x167F },
    { 0x1681, 0x169A },
    { 0x16A0, 0x16EA },
    { 0x16EE, 0x16F0 },
    { 0x16F1, 0x16F8 },
    { 0x1700, 0x1711 },
    { 0x171F, 0x1731 },
    { 0x1740, 0x1751 },
    { 0x1760, 0x176C },
    { 0x176E, 0x1770 },
    { 0x1780, 0x17B3 },
    { 0x17D7, 0x17D7 },
    { 0x17DC, 0x17DC },
    { 0x1820, 0x1842 },
    { 0x1843, 0x1843 },
    { 0x1844, 0x1878 },
    { 0x1880, 0x1884 },
    { 0x1885, 0x1886 },
    { 0x1887, 0x18A8 },
    { 0x18AA, 0x18AA },
    { 0x18B0, 0x18F5 },
    { 0x1900, 0x191E },
    { 0x1950, 0x196D },
    { 0x1970, 0x1974 },
    { 0x1980, 0x19AB },
    { 0x19B0, 0x19C9 },
    { 0x1A00, 0x1A16 },
    { 0x1A20, 0x1A54 },
    { 0x1AA7, 0x1AA7 },
    { 0x1B05, 0x1B33 },
    { 0x1B45, 0x1B4C },
    { 0x1B83, 0x1BA0 },
    { 0x1BAE, 0x1BAF },
    { 0x1BBA, 0x1BE5 },
    { 0x1C00, 0x1C23 },
    { 0x1C4D, 0x1C4F },
    { 0x1C5A, 0x1C77 },
    { 0x1C78, 0x1C7D },
    { 0x1C80, 0x1C88 },
    { 0x1C90, 0x1CBA },
    { 0x1CBD, 0x1CBF },
    { 0x1CE9, 0x1CEC },
    { 0x1CEE, 0x1CF3 },
    { 0x1CF5, 0x1CF6 },
    { 0x1CFA, 0x1CFA },
    { 0x1D00, 0x1D2B },
    { 0x1D2C, 0x1D6A },
    { 0x1D6B, 0x1D77 },
    { 0x1D78, 0x1D78 },
    { 0x1D79, 0x1D9A },
    { 0x1D9B, 0x1DBF },
    { 0x1E00, 0x1F15 },
    { 0x1F18, 0x1F1D },
    { 0x1F20, 0x1F45 },
    { 0x1F48, 0x1F4D },
    { 0x1F50, 0x1F57 },
    { 0x1F59, 0x1F59 },
    { 0x1F5B, 0x1F5B },
    { 0x1F5D, 0x1F5D },
    { 0x1F5F, 0x1F7D },
    { 0x1F80, 0x1FB4 },
    { 0x1FB6, 0x1FBC },
    { 0x1FBE, 0x1FBE },
    { 0x1FC2, 0x1FC4 },
    { 0x1FC6, 0x1FCC },
    { 0x1FD0, 0x1FD3 },
    { 0x1FD6, 0x1FDB },
    { 0x1FE0, 0x1FEC },
    { 0x1FF2, 0x1FF4 },
    { 0x1FF6, 0x1FFC },
    { 0x2071, 0x2071 },
    { 0x207F, 0x207F },
    { 0x2090, 0x209C },
    { 0x2102, 0x2102 },
    { 0x2107, 0x2107 },
    { 0x210A, 0x2113 },
    { 0x2115, 0x2115 },
    { 0x2118, 0x2118 },
    { 0x2119, 0x211D },
    { 0x2124, 0x2124 },
    { 0x2126, 0x2126 },
    { 0x2128, 0x2128 },
    { 0x212A, 0x212D },
    { 0x212E, 0x212E },
    { 0x212F, 0x2134 },
    { 0x2135, 0x2138 },
    { 0x2139, 0x2139 },
    { 0x213C, 0x213F },
    { 0x2145, 0x2149 },
    { 0x214E, 0x214E },
    { 0x2160, 0x2182 },
    { 0x2183, 0x2184 },
    { 0x2185, 0x2188 },
    { 0x2C00, 0x2C7B },
    { 0x2C7C, 0x2C7D },
    { 0x2C7E, 0x2CE4 },
    { 0x2CEB, 0x2CEE },
    { 0x2CF2, 0x2CF3 },
    { 0x2D00, 0x2D25 },
    { 0x2D27, 0x2D27 },
    { 0x2D2D, 0x2D2D },
    { 0x2D30, 0x2D67 },
    { 0x2D6F, 0x2D6F },
    { 0x2D80, 0x2D96 },
    { 0x2DA0, 0x2DA6 },
    { 0x2DA8, 0x2DAE },
    { 0x2DB0, 0x2DB6 },
    { 0x2DB8, 0x2DBE },
    { 0x2DC0, 0x2DC6 },
    { 0x2DC8, 0x2DCE },
    { 0x2DD0, 0x2DD6 },
    { 0x2DD8, 0x2DDE },
    { 0x3005, 0x3005 },
    { 0x3006, 0x3006 },
    { 0x3007, 0x3007 },
    { 0x3021, 0x3029 },
    { 0x3031, 0x3035 },
    { 0x3038, 0x303A },
    { 0x303B, 0x303B },
    { 0x303C, 0x303C },
    { 0x3041, 0x3096 },
    { 0x309D, 0x309E },
    { 0x309F, 0x309F },
    { 0x30A1, 0x30FA },
    { 0x30FC, 0x30FE },
    { 0x30FF, 0x30FF },
    { 0x3105, 0x312F },
    { 0x3131, 0x318E },
    { 0x31A0, 0x31BF },
    { 0x31F0, 0x31FF },
    { 0x3400, 0x4DBF },
    { 0x4E00, 0xA014 },
    { 0xA015, 0xA015 },
    { 0xA016, 0xA48C },
    { 0xA4D0, 0xA4F7 },
    { 0xA4F8, 0xA4FD },
    { 0xA500, 0xA60B },
    { 0xA60C, 0xA60C },
    { 0xA610, 0xA61F },
    { 0xA62A, 0xA62B },
    { 0xA640, 0xA66D },
    { 0xA66E, 0xA66E },
    { 0xA67F, 0xA67F },
    { 0xA680, 0xA69B },
    { 0xA69C, 0xA69D },
    { 0xA6A0, 0xA6E5 },
    { 0xA6E6, 0xA6EF },
    { 0xA717, 0xA71F },
    { 0xA722, 0xA76F },
    { 0xA770, 0xA770 },
    { 0xA771, 0xA787 },
    { 0xA788, 0xA788 },
    { 0xA78B, 0xA78E },
    { 0xA78F, 0xA78F },
    { 0xA790, 0xA7CA },
    { 0xA7D0, 0xA7D1 },
    { 0xA7D3, 0xA7D3 },
    { 0xA7D5, 0xA7D9 },
    { 0xA7F2, 0xA7F4 },
    { 0xA7F5, 0xA7F6 },
    { 0xA7F7, 0xA7F7 },
    { 0xA7F8, 0xA7F9 },
    { 0xA7FA, 0xA7FA },
    { 0xA7FB, 0xA801 },
    { 0xA803, 0xA805 },
    { 0xA807, 0xA80A },
    { 0xA80C, 0xA822 },
    { 0xA840, 0xA873 },
    { 0xA882, 0xA8B3 },
    { 0xA8F2, 0xA8F7 },
    { 0xA8FB, 0xA8FB },
    { 0xA8FD, 0xA8FE },
    { 0xA90A, 0xA925 },
    { 0xA930, 0xA946 },
    { 0xA960, 0xA97C },
    { 0xA984, 0xA9B2 },
    { 0xA9CF, 0xA9CF },
    { 0xA9E0, 0xA9E4 },
    { 0xA9E6, 0xA9E6 },
    { 0xA9E7, 0xA9EF },
    { 0xA9FA, 0xA9FE },
    { 0xAA00, 0xAA28 },
    { 0xAA40, 0xAA42 },
    { 0xAA44, 0xAA4B },
    { 0xAA60, 0xAA6F },
    { 0xAA70, 0xAA70 },
    { 0xAA71, 0xAA76 },
    { 0xAA7A, 0xAA7A },
    { 0xAA7E, 0xAAAF },
    { 0xAAB1, 0xAAB1 },
    { 0xAAB5, 0xAAB6 },
    { 0xAAB9, 0xAABD },
    { 0xAAC0, 0xAAC0 },
    { 0xAAC2, 0xAAC2 },
    { 0xAADB, 0xAADC },
    { 0xAADD, 0xAADD },
    { 0xAAE0, 0xAAEA },
    { 0xAAF2, 0xAAF2 },
    { 0xAAF3, 0xAAF4 },
    { 0xAB01, 0xAB06 },
    { 0xAB09, 0xAB0E },
    { 0xAB11, 0xAB16 },
    { 0xAB20, 0xAB26 },
    { 0xAB28, 0xAB2E },
    { 0xAB30, 0xAB5A },
    { 0xAB5C, 0xAB5F },
    { 0xAB60, 0xAB68 },
    { 0xAB69, 0xAB69 },
    { 0xAB70, 0xABBF },
    { 0xABC0, 0xABE2 },
    { 0xAC00, 0xD7A3 },
    { 0xD7B0, 0xD7C6 },
    { 0xD7CB, 0xD7FB },
    { 0xF900, 0xFA6D },
    { 0xFA70, 0xFAD9 },
    { 0xFB00, 0xFB06 },
    { 0xFB13, 0xFB17 },
    { 0xFB1D, 0xFB1D },
    { 0xFB1F, 0xFB28 },
    { 0xFB2A, 0xFB36 },
    { 0xFB38, 0xFB3C },
    { 0xFB3E, 0xFB3E },
    { 0xFB40, 0xFB41 },
    { 0xFB43, 0xFB44 },
    { 0xFB46, 0xFBB1 },
    { 0xFBD3, 0xFC5D },
    { 0xFC64, 0xFD3D },
    { 0xFD50, 0xFD8F },
    { 0xFD92, 0xFDC7 },
    { 0xFDF0, 0xFDF9 },
    { 0xFE71, 0xFE71 },
    { 0xFE73, 0xFE73 },
    { 0xFE77, 0xFE77 },
    { 0xFE79, 0xFE79 },
    { 0xFE7B, 0xFE7B },
    { 0xFE7D, 0xFE7D },
    { 0xFE7F, 0xFEFC },
    { 0xFF21, 0xFF3A },
    { 0xFF41, 0xFF5A },
    { 0xFF66, 0xFF6F },
    { 0xFF70, 0xFF70 },
    { 0xFF71, 0xFF9D },
    { 0xFFA0, 0xFFBE },
    { 0xFFC2, 0xFFC7 },
    { 0xFFCA, 0xFFCF },
    { 0xFFD2, 0xFFD7 },
    { 0xFFDA, 0xFFDC },
    { 0x10000, 0x1000B },
    { 0x1000D, 0x10026 },
    { 0x10028, 0x1003A },
    { 0x1003C, 0x1003D },
    { 0x1003F, 0x1004D },
    { 0x10050, 0x1005D },
    { 0x10080, 0x100FA },
    { 0x10140, 0x10174 },
    { 0x10280, 0x1029C },
    { 0x102A0, 0x102D0 },
    { 0x10300, 0x1031F },
    { 0x1032D, 0x10340 },
    { 0x10341, 0x10341 },
    { 0x10342, 0x10349 },
    { 0x1034A, 0x1034A },
    { 0x10350, 0x10375 },
    { 0x10380, 0x1039D },
    { 0x103A0, 0x103C3 },
    { 0x103C8, 0x103CF },
    { 0x103D1, 0x103D5 },
    { 0x10400, 0x1044F },
    { 0x10450, 0x1049D },
    { 0x104B0, 0x104D3 },
    { 0x104D8, 0x104FB },
    { 0x10500, 0x10527 },
    { 0x10530, 0x10563 },
    { 0x10570, 0x1057A },
    { 0x1057C, 0x1058A },
    { 0x1058C, 0x10592 },
    { 0x10594, 0x10595 },
    { 0x10597, 0x105A1 },
    { 0x105A3, 0x105B1 },
    { 0x105B3, 0x105B9 },
    { 0x105BB, 0x105BC },
    { 0x10600, 0x10736 },
    { 0x10740, 0x10755 },
    { 0x10760, 0x10767 },
    { 0x10780, 0x10785 },
    { 0x10787, 0x107B0 },
    { 0x107B2, 0x107BA },
    { 0x10800, 0x10805 },
    { 0x10808, 0x10808 },
    { 0x1080A, 0x10835 },
    { 0x10837, 0x10838 },
    { 0x1083C, 0x1083C },
    { 0x1083F, 0x10855 },
    { 0x10860, 0x10876 },
    { 0x10880, 0x1089E },
    { 0x108E0, 0x108F2 },
    { 0x108F4, 0x108F5 },
    { 0x10900, 0x10915 },
    { 0x10920, 0x10939 },
    { 0x10980, 0x109B7 },
    { 0x109BE, 0x109BF },
    { 0x10A00, 0x10A00 },
    { 0x10A10, 0x10A13 },
    { 0x10A15, 0x10A17 },
    { 0x10A19, 0x10A35 },
    { 0x10A60, 0x10A7C },
    { 0x10A80, 0x10A9C },
    { 0x10AC0, 0x10AC7 },
    { 0x10AC9, 0x10AE4 },
    { 0x10B00, 0x10B35 },
    { 0x10B40, 0x10B55 },
    { 0x10B60, 0x10B72 },
    { 0x10B80, 0x10B91 },
    { 0x10C00, 0x10C48 },
    { 0x10C80, 0x10CB2 },
    { 0x10CC0, 0x10CF2 },
    { 0x10D00, 0x10D23 },
    { 0x10E80, 0x10EA9 },
    { 0x10EB0, 0x10EB1 },
    { 0x10F00, 0x10F1C },
    { 0x10F27, 0x10F27 },
    { 0x10F30, 0x10F45 },
    { 0x10F70, 0x10F81 },
    { 0x10FB0, 0x10FC4 },
    { 0x10FE0, 0x10FF6 },
    { 0x11003, 0x11037 },
    { 0x11071, 0x11072 },
    { 0x11075, 0x11075 },
    { 0x11083, 0x110AF },
    { 0x110D0, 0x110E8 },
    { 0x11103, 0x11126 },
    { 0x11144, 0x11144 },
    { 0x11147, 0x11147 },
    { 0x11150, 0x11172 },
    { 0x11176, 0x11176 },
    { 0x11183, 0x111B2 },
    { 0x111C1, 0x111C4 },
    { 0x111DA, 0x111DA },
    { 0x111DC, 0x111DC },
    { 0x11200, 0x11211 },
    { 0x11213, 0x1122B },
    { 0x11280, 0x11286 },
    { 0x11288, 0x11288 },
    { 0x1128A, 0x1128D },
    { 0x1128F, 0x1129D },
    { 0x1129F, 0x112A8 },
    { 0x112B0, 0x112DE },
    { 0x11305, 0x1130C },
    { 0x1130F, 0x11310 },
    { 0x11313, 0x11328 },
    { 0x1132A, 0x11330 },
    { 0x11332, 0x11333 },
    { 0x11335, 0x11339 },
    { 0x1133D, 0x1133D },
    { 0x11350, 0x11350 },
    { 0x1135D, 0x11361 },
    { 0x11400, 0x11434 },
    { 0x11447, 0x1144A },
    { 0x1145F, 0x11461 },
    { 0x11480, 0x114AF },
    { 0x114C4, 0x114C5 },
    { 0x114C7, 0x114C7 },
    { 0x11580, 0x115AE },
    { 0x115D8, 0x115DB },
    { 0x11600, 0x1162F },
    { 0x11644, 0x11644 },
    { 0x11680, 0x116AA },
    { 0x116B8, 0x116B8 },
    { 0x11700, 0x1171A },
    { 0x11740, 0x11746 },
    { 0x11800, 0x1182B },
    { 0x118A0, 0x118DF },
    { 0x118FF, 0x11906 },
    { 0x11909, 0x11909 },
    { 0x1190C, 0x11913 },
    { 0x11915, 0x11916 },
    { 0x11918, 0x1192F },
    { 0x1193F, 0x1193F },
    { 0x11941, 0x11941 },
    { 0x119A0, 0x119A7 },
    { 0x119AA, 0x119D0 },
    { 0x119E1, 0x119E1 },
    { 0x119E3, 0x119E3 },
    { 0x11A00, 0x11A00 },
    { 0x11A0B, 0x11A32 },
    { 0x11A3A, 0x11A3A },
    { 0x11A50, 0x11A50 },
    { 0x11A5C, 0x11A89 },
    { 0x11A9D, 0x11A9D },
    { 0x11AB0, 0x11AF8 },
    { 0x11C00, 0x11C08 },
    { 0x11C0A, 0x11C2E },
    { 0x11C40, 0x11C40 },
    { 0x11C72, 0x11C8F },
    { 0x11D00, 0x11D06 },
    { 0x11D08, 0x11D09 },
    { 0x11D0B, 0x11D30 },
    { 0x11D46, 0x11D46 },
    { 0x11D60, 0x11D65 },
    { 0x11D67, 0x11D68 },
    { 0x11D6A, 0x11D89 },
    { 0x11D98, 0x11D98 },
    { 0x11EE0, 0x11EF2 },
    { 0x11FB0, 0x11FB0 },
    { 0x12000, 0x12399 },
    { 0x12400, 0x1246E },
    { 0x12480, 0x12543 },
    { 0x12F90, 0x12FF0 },
    { 0x13000, 0x1342E },
    { 0x14400, 0x14646 },
    { 0x16800, 0x16A38 },
    { 0x16A40, 0x16A5E },
    { 0x16A70, 0x16ABE },
    { 0x16AD0, 0x16AED },
    { 0x16B00, 0x16B2F },
    { 0x16B40, 0x16B43 },
    { 0x16B63, 0x16B77 },
    { 0x16B7D, 0x16B8F },
    { 0x16E40, 0x16E7F },
    { 0x16F00, 0x16F4A },
    { 0x16F50, 0x16F50 },
    { 0x16F93, 0x16F9F },
    { 0x16FE0, 0x16FE1 },
    { 0x16FE3, 0x16FE3 },
    { 0x17000, 0x187F7 },
    { 0x18800, 0x18CD5 },
    { 0x18D00, 0x18D08 },
    { 0x1AFF0, 0x1AFF3 },
    { 0x1AFF5, 0x1AFFB },
    { 0x1AFFD, 0x1AFFE },
    { 0x1B000, 0x1B122 },
    { 0x1B150, 0x1B152 },
    { 0x1B164, 0x1B167 },
    { 0x1B170, 0x1B2FB },
    { 0x1BC00, 0x1BC6A },
    { 0x1BC70, 0x1BC7C },
    { 0x1BC80, 0x1BC88 },
    { 0x1BC90, 0x1BC99 },
    { 0x1D400, 0x1D454 },
    { 0x1D456, 0x1D49C },
    { 0x1D49E, 0x1D49F },
    { 0x1D4A2, 0x1D4A2 },
    { 0x1D4A5, 0x1D4A6 },
    { 0x1D4A9, 0x1D4AC },
    { 0x1D4AE, 0x1D4B9 },
    { 0x1D4BB, 0x1D4BB },
    { 0x1D4BD, 0x1D4C3 },
    { 0x1D4C5, 0x1D505 },
    { 0x1D507, 0x1D50A },
    { 0x1D50D, 0x1D514 },
    { 0x1D516, 0x1D51C },
    { 0x1D51E, 0x1D539 },
    { 0x1D53B, 0x1D53E },
    { 0x1D540, 0x1D544 },
    { 0x1D546, 0x1D546 },
    { 0x1D54A, 0x1D550 },
    { 0x1D552, 0x1D6A5 },
    { 0x1D6A8, 0x1D6C0 },
    { 0x1D6C2, 0x1D6DA },
    { 0x1D6DC, 0x1D6FA },
    { 0x1D6FC, 0x1D714 },
    { 0x1D716, 0x1D734 },
    { 0x1D736, 0x1D74E },
    { 0x1D750, 0x1D76E },
    { 0x1D770, 0x1D788 },
    { 0x1D78A, 0x1D7A8 },
    { 0x1D7AA, 0x1D7C2 },
    { 0x1D7C4, 0x1D7CB },
    { 0x1DF00, 0x1DF09 },
    { 0x1DF0A, 0x1DF0A },
    { 0x1DF0B, 0x1DF1E },
    { 0x1E100, 0x1E12C },
    { 0x1E137, 0x1E13D },
    { 0x1E14E, 0x1E14E },
    { 0x1E290, 0x1E2AD },
    { 0x1E2C0, 0x1E2EB },
    { 0x1E7E0, 0x1E7E6 },
    { 0x1E7E8, 0x1E7EB },
    { 0x1E7ED, 0x1E7EE },
    { 0x1E7F0, 0x1E7FE },
    { 0x1E800, 0x1E8C4 },
    { 0x1E900, 0x1E943 },
    { 0x1E94B, 0x1E94B },
    { 0x1EE00, 0x1EE03 },
    { 0x1EE05, 0x1EE1F },
    { 0x1EE21, 0x1EE22 },
    { 0x1EE24, 0x1EE24 },
    { 0x1EE27, 0x1EE27 },
    { 0x1EE29, 0x1EE32 },
    { 0x1EE34, 0x1EE37 },
    { 0x1EE39, 0x1EE39 },
    { 0x1EE3B, 0x1EE3B },
    { 0x1EE42, 0x1EE42 },
    { 0x1EE47, 0x1EE47 },
    { 0x1EE49, 0x1EE49 },
    { 0x1EE4B, 0x1EE4B },
    { 0x1EE4D, 0x1EE4F },
    { 0x1EE51, 0x1EE52 },
    { 0x1EE54, 0x1EE54 },
    { 0x1EE57, 0x1EE57 },
    { 0x1EE59, 0x1EE59 },
    { 0x1EE5B, 0x1EE5B },
    { 0x1EE5D, 0x1EE5D },
    { 0x1EE5F, 0x1EE5F },
    { 0x1EE61, 0x1EE62 },
    { 0x1EE64, 0x1EE64 },
    { 0x1EE67, 0x1EE6A },
    { 0x1EE6C, 0x1EE72 },
    { 0x1EE74, 0x1EE77 },
    { 0x1EE79, 0x1EE7C },
    { 0x1EE7E, 0x1EE7E },
    { 0x1EE80, 0x1EE89 },
    { 0x1EE8B, 0x1EE9B },
    { 0x1EEA1, 0x1EEA3 },
    { 0x1EEA5, 0x1EEA9 },
    { 0x1EEAB, 0x1EEBB },
    { 0x20000, 0x2A6DF },
    { 0x2A700, 0x2B738 },
    { 0x2B740, 0x2B81D },
    { 0x2B820, 0x2CEA1 },
    { 0x2CEB0, 0x2EBE0 },
    { 0x2F800, 0x2FA1D },
    { 0x30000, 0x3134A },
};

// DerivedCoreProperties.txt, XID_Continue
constexpr uint32_t xid_continue_table[][2] = {
    { 0x0030, 0x0039 },
    { 0x0041, 0x005A },
    { 0x005F, 0x005F },
    { 0x0061, 0x007A },
    { 0x00AA, 0x00AA },
    { 0x00B5, 0x00B5 },
    { 0x00B7, 0x00B7 },
    { 0x00BA, 0x00BA },
    { 0x00C0, 0x00D6 },
    { 0x00D8, 0x00F6 },
    { 0x00F8, 0x01BA },
    { 0x01BB, 0x01BB },
    { 0x01BC, 0x01BF },
    { 0x01C0, 0x01C3 },
    { 0x01C4, 0x0293 },
    { 0x0294, 0x0294 },
    { 0x0295, 0x02AF },
    { 0x02B0, 0x02C1 },
    { 0x02C6, 0x02D1 },
    { 0x02E0, 0x02E4 },
    { 0x02EC, 0x02EC },
    { 0x02EE, 0x02EE },
    { 0x0300, 0x036F },
    { 0x0370, 0x0373 },
    { 0x0374, 0x0374 },
    { 0x0376, 0x0377 },
    { 0x037B, 0x037D },
    { 0x037F, 0x037F },
    { 0x0386, 0x0386 },
    { 0x0387, 0x0387 },
    { 0x0388, 0x038A },
    { 0x038C, 0x038C },
    { 0x038E, 0x03A1 },
    { 0x03A3, 0x03F5 },
    { 0x03F7, 0x0481 },
    { 0x0483, 0x0487 },
    { 0x048A, 0x052F },
    { 0x0531, 0x0556 },
    { 0x0559, 0x0559 },
    { 0x0560, 0x0588 },
    { 0x0591, 0x05BD },
    { 0x05BF, 0x05BF },
    { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C5 },
    { 0x05C7, 0x05C7 },
    { 0x05D0, 0x05EA },
    { 0x05EF, 0x05F2 },
    { 0x0610, 0x061A },
    { 0x0620, 0x063F },
    { 0x0640, 0x0640 },
    { 0x0641, 0x064A },
    { 0x064B, 0x065F },
    { 0x0660, 0x0669 },
    { 0x066E, 0x066F },
    { 0x0670, 0x0670 },
    { 0x0671, 0x06D3 },
    { 0x06D5, 0x06D5 },
    { 0x06D6, 0x06DC },
    { 0x06DF, 0x06E4 },
    { 0x06E5, 0x06E6 },
    { 0x06E7, 0x06E8 },
    { 0x06EA, 0x06ED },
    { 0x06EE, 0x06EF },
    { 0x06F0, 0x06F9 },
    { 0x06FA, 0x06FC },
    { 0x06FF, 0x06FF },
    { 0x0710, 0x0710 },
    { 0x0711, 0x0711 },
    { 0x0712, 0x072F },
    { 0x0730, 0x074A },
    { 0x074D, 0x07A5 },
    { 0x07A6, 0x07B0 },
    { 0x07B1, 0x07B1 },
    { 0x07C0, 0x07C9 },
    { 0x07CA, 0x07EA },
    { 0x07EB, 0x07F3 },
    { 0x07F4, 0x07F5 },
    { 0x07FA, 0x07FA },
    { 0x07FD, 0x07FD },
    { 0x0800, 0x0815 },
    { 0x0816, 0x0819 },
    { 0x081A, 0x081A },
    { 0x081B, 0x0823 },
    { 0x0824, 0x0824 },
    { 0x0825, 0x0827 },
    { 0x0828, 0x0828 },
    { 0x0829, 0x082D },
    { 0x0840, 0x0858 },
    { 0x0859, 0x085B },
    { 0x0860, 0x086A },
    { 0x0870, 0x0887 },
    { 0x0889, 0x088E },
    { 0x0898, 0x089F },
    { 0x08A0, 0x08C8 },
    { 0x08C9, 0x08C9 },
    { 0x08CA, 0x08E1 },
    { 0x08E3, 0x0902 },
    { 0x0903, 0x0903 },
    { 0x0904, 0x0939 },
    { 0x093A, 0x093A },
    { 0x093B, 0x093B },
    { 0x093C, 0x093C },
    { 0x093D, 0x093D },
    { 0x093E, 0x0940 },
    { 0x0941, 0x0948 },
    { 0x0949, 0x094C },
    { 0x094D, 0x094D },
    { 0x094E, 0x094F },
    { 0x0950, 0x0950 },
    { 0x0951, 0x0957 },
    { 0x0958, 0x0961 },
    { 0x0962, 0x0963 },
    { 0x0966, 0x096F },
    { 0x0971, 0x0971 },
    { 0x0972, 0x0980 },
    { 0x0981, 0x0981 },
    { 0x0982, 0x0983 },
    { 0x0985, 0x098C },
    { 0x098F, 0x0990 },
    { 0x0993, 0x09A8 },
    { 0x09AA, 0x09B0 },
    { 0x09B2, 0x09B2 },
    { 0x09B6, 0x09B9 },
    { 0x09BC, 0x09BC },
    { 0x09BD, 0x09BD },
    { 0x09BE, 0x09C0 },
    { 0x09C1, 0x09C4 },
    { 0x09C7, 0x09C8 },
    { 0x09CB, 0x09CC },
    { 0x09CD, 0x09CD },
    { 0x09CE, 0x09CE },
    { 0x09D7, 0x09D7 },
    { 0x09DC, 0x09DD },
    { 0x09DF, 0x09E1 },
    { 0x09E2, 0x09E3 },
    { 0x09E6, 0x09EF },
    { 0x09F0, 0x09F1 },
    { 0x09FC, 0x09FC },
    { 0x09FE, 0x09FE },
    { 0x0A01, 0x0A02 },
    { 0x0A03, 0x0A03 },
    { 0x0A05, 0x0A0A },
    { 0x0A0F, 0x0A10 },
    { 0x0A13, 0x0A28 },
    { 0x0A2A, 0x0A30 },
    { 0x0A32, 0x0A33 },
    { 0x0A35, 0x0A36 },
    { 0x0A38, 0x0A39 },
    { 0x0A3C, 0x0A3C },
    { 0x0A3E, 0x0A40 },
    { 0x0A41, 0x0A42 },
    { 0x0A47, 0x0A48 },
    { 0x0A4B, 0x0A4D },
    { 0x0A51, 0x0A51 },
    { 0x0A59, 0x0A5C },
    { 0x0A5E, 0x0A5E },
    { 0x0A66, 0x0A6F },
    { 0x0A70, 0x0A71 },
    { 0x0A72, 0x0A74 },
    { 0x0A75, 0x0A75 },
    { 0x0A81, 0x0A82 },
    { 0x0A83, 0x0A83 },
    { 0x0A85, 0x0A8D },
    { 0x0A8F, 0x0A91 },
    { 0x0A93, 0x0AA8 },
    { 0x0AAA, 0x0AB0 },
    { 0x0AB2, 0x0AB3 },
    { 0x0AB5, 0x0AB9 },
    { 0x0ABC, 0x0ABC },
    { 0x0ABD, 0x0ABD },
    { 0x0ABE, 0x0AC0 },
    { 0x0AC1, 0x0AC5 },
    { 0x0AC7, 0x0AC8 },
    { 0x0AC9, 0x0AC9 },
    { 0x0ACB, 0x0ACC },
    { 0x0ACD, 0x0ACD },
    { 0x0AD0, 0x0AD0 },
    { 0x0AE0, 0x0AE1 },
    { 0x0AE2, 0x0AE3 },
    { 0x0AE6, 0x0AEF },
    { 0x0AF9, 0x0AF9 },
    { 0x0AFA, 0x0AFF },
    { 0x0B01, 0x0B01 },
    { 0x0B02, 0x0B03 },
    { 0x0B05, 0x0B0C },
    { 0x0B0F, 0x0B10 },
    { 0x0B13, 0x0B28 },
    { 0x0B2A, 0x0B30 },
    { 0x0B32, 0x0B33 },
    { 0x0B35, 0x0B39 },
    { 0x0B3C, 0x0B3C },
    { 0x0B3D, 0x0B3D },
    { 0x0B3E, 0x0B3E },
    { 0x0B3F, 0x0B3F },
    { 0x0B40, 0x0B40 },
    { 0x0B41, 0x0B44 },
    { 0x0B47, 0x0B48 },
    { 0x0B4B, 0x0B4C },
    { 0x0B4D, 0x0B4D },
    { 0x0B55, 0x0B56 },
    { 0x0B57, 0x0B57 },
    { 0x0B5C, 0x0B5D },
    { 0x0B5F, 0x0B61 },
    { 0x0B62, 0x0B63 },
    { 0x0B66, 0x0B6F },
    { 0x0B71, 0x0B71 },
    { 0x0B82, 0x0B82 },
    { 0x0B83, 0x0B83 },
    { 0x0B85, 0x0B8A },
    { 0x0B8E, 0x0B90 },
    { 0x0B92, 0x0B95 },
    { 0x0B99, 0x0B9A },
    { 0x0B9C, 0x0B9C },
    { 0x0B9E, 0x0B9F },
    { 0x0BA3, 0x0BA4 },
    { 0x0BA8, 0x0BAA },
    { 0x0BAE, 0x0BB9 },
    { 0x0BBE, 0x0BBF },
    { 0x0BC0, 0x0BC0 },
    { 0x0BC1, 0x0BC2 },
    { 0x0BC6, 0x0BC8 },
    { 0x0BCA, 0x0BCC },
    { 0x0BCD, 0x0BCD },
    { 0x0BD0, 0x0BD0 },
    { 0x0BD7, 0x0BD7 },
    { 0x0BE6, 0x0BEF },
    { 0x0C00, 0x0C00 },
    { 0x0C01, 0x0C03 },
    { 0x0C04, 0x0C04 },
    { 0x0C05, 0x0C0C },
    { 0x0C0E, 0x0C10 },
    { 0x0C12, 0x0C28 },
    { 0x0C2A, 0x0C39 },
    { 0x0C3C, 0x0C3C },
    { 0x0C3D, 0x0C3D },
    { 0x0C3E, 0x0C40 },
    { 0x0C41, 0x0C44 },
    { 0x0C46, 0x0C48 },
    { 0x0C4A, 0x0C4D },
    { 0x0C55, 0x0C56 },
    { 0x0C58, 0x0C5A },
    { 0x0C5D, 0x0C5D },
    { 0x0C60, 0x0C61 },
    { 0x0C62, 0x0C63 },
    { 0x0C66, 0x0C6F },
    { 0x0C80, 0x0C80 },
    { 0x0C81, 0x0C81 },
    { 0x0C82, 0x0C83 },
    { 0x0C85, 0x0C8C },
    { 0x0C8E, 0x0C90 },
    { 0x0C92, 0x0CA8 },
    { 0x0CAA, 0x0CB3 },
    { 0x0CB5, 0x0CB9 },
    { 0x0CBC, 0x0CBC },
    { 0x0CBD, 0x0CBD },
    { 0x0CBE, 0x0CBE },
    { 0x0CBF, 0x0CBF },
    { 0x0CC0, 0x0CC4 },
    { 0x0CC6, 0x0CC6 },
    { 0x0CC7, 0x0CC8 },
    { 0x0CCA, 0x0CCB },
    { 0x0CCC, 0x0CCD },
    { 0x0CD5, 0x0CD6 },
    { 0x0CDD, 0x0CDE },
    { 0x0CE0, 0x0CE1 },
    { 0x0CE2, 0x0CE3 },
    { 0x0CE6, 0x0CEF },
    { 0x0CF1, 0x0CF2 },
    { 0x0D00, 0x0D01 },
    { 0x0D02, 0x0D03 },
    { 0x0D04, 0x0D0C },
    { 0x0D0E, 0x0D10 },
    { 0x0D12, 0x0D3A },
    { 0x0D3B, 0x0D3C },
    { 0x0D3D, 0x0D3D },
    { 0x0D3E, 0x0D40 },
    { 0x0D41, 0x0D44 },
    { 0x0D46, 0x0D48 },
    { 0x0D4A, 0x0D4C },
    { 0x0D4D, 0x0D4D },
    { 0x0D4E, 0x0D4E },
    { 0x0D54, 0x0D56 },
    { 0x0D57, 0x0D57 },
    { 0x0D5F, 0x0D61 },
    { 0x0D62, 0x0D63 },
    { 0x0D66, 0x0D6F },
    { 0x0D7A, 0x0D7F },
    { 0x0D81, 0x0D81 },
    { 0x0D82, 0x0D83 },
    { 0x0D85, 0x0D96 },
    { 0x0D9A, 0x0DB1 },
    { 0x0DB3, 0x0DBB },
    { 0x0DBD, 0x0DBD },
    { 0x0DC0, 0x0DC6 },
    { 0x0DCA, 0x0DCA },
    { 0x0DCF, 0x0DD1 },
    { 0x0DD2, 0x0DD4 },
    { 0x0DD6, 0x0DD6 },
    { 0x0DD8, 0x0DDF },
    { 0x0DE6, 0x0DEF },
    { 0x0DF2, 0x0DF3 },
    { 0x0E01, 0x0E30 },
    { 0x0E31, 0x0E31 },
    { 0x0E32, 0x0E33 },
    { 0x0E34, 0x0E3A },
    { 0x0E40, 0x0E45 },
    { 0x0E46, 0x0E46 },
    { 0x0E47, 0x0E4E },
    { 0x0E50, 0x0E59 },
    { 0x0E81, 0x0E82 },
    { 0x0E84, 0x0E84 },
    { 0x0E86, 0x0E8A },
    { 0x0E8C, 0x0EA3 },
    { 0x0EA5, 0x0EA5 },
    { 0x0EA7, 0x0EB0 },
    { 0x0EB1, 0x0EB1 },
    { 0x0EB2, 0x0EB3 },
    { 0x0EB4, 0x0EBC },
    { 0x0EBD, 0x0EBD },
    { 0x0EC0, 0x0EC4 },
    { 0x0EC6, 0x0EC6 },
    { 0x0EC8, 0x0ECD },
    { 0x0ED0, 0x0ED9 },
    { 0x0EDC, 0x0EDF },
    { 0x0F00, 0x0F00 },
    { 0x0F18, 0x0F19 },
    { 0x0F20, 0x0F29 },
    { 0x0F35, 0x0F35 },
    { 0x0F37, 0x0F37 },
    { 0x0F39, 0x0F39 },
    { 0x0F3E, 0x0F3F },
    { 0x0F40, 0x0F47 },
    { 0x0F49, 0x0F6C },
    { 0x0F71, 0x0F7E },
    { 0x0F7F, 0x0F7F },
    { 0x0F80, 0x0F84 },
    { 0x0F86, 0x0F87 },
    { 0x0F88, 0x0F8C },
    { 0x0F8D, 0x0F97 },
    { 0x0F99, 0x0FBC },
    { 0x0FC6, 0x0FC6 },
    { 0x1000, 0x102A },
    { 0x102B, 0x102C },
    { 0x102D, 0x1030 },
    { 0x1031, 0x1031 },
    { 0x1032, 0x1037 },
    { 0x1038, 0x1038 },
    { 0x1039, 0x103A },
    { 0x103B, 0x103C },
    { 0x103D, 0x103E },
    { 0x103F, 0x103F },
    { 0x1040, 0x1049 },
    { 0x1050, 0x1055 },
    { 0x1056, 0x1057 },
    { 0x1058, 0x1059 },
    { 0x105A, 0x105D },
    { 0x105E, 0x1060 },
    { 0x1061, 0x1061 },
    { 0x1062, 0x1064 },
    { 0x1065, 0x1066 },
    { 0x1067, 0x106D },
    { 0x106E, 0x1070 },
    { 0x1071, 0x1074 },
    { 0x1075, 0x1081 },
    { 0x1082, 0x1082 },
    { 0x1083, 0x1084 },
    { 0x1085, 0x1086 },
    { 0x1087, 0x108C },
    { 0x108D, 0x108D },
    { 0x108E, 0x108E },
    { 0x108F, 0x108F },
    { 0x1090, 0x1099 },
    { 0x109A, 0x109C },
    { 0x109D, 0x109D },
    { 0x10A0, 0x10C5 },
    { 0x10C7, 0x10C7 },
    { 0x10CD, 0x10CD },
    { 0x10D0, 0x10FA },
    { 0x10FC, 0x10FC },
    { 0x10FD, 0x10FF },
    { 0x1100, 0x1248 },
    { 0x124A, 0x124D },
    { 0x1250, 0x1256 },
    { 0x1258, 0x1258 },
    { 0x125A, 0x125D },
    { 0x1260, 0x1288 },
    { 0x128A, 0x128D },
    { 0x1290, 0x12B0 },
    { 0x12B2, 0x12B5 },
    { 0x12B8, 0x12BE },
    { 0x12C0, 0x12C0 },
    { 0x12C2, 0x12C5 },
    { 0x12C8, 0x12D6 },
    { 0x12D8, 0x1310 },
    { 0x1312, 0x1315 },
    { 0x1318, 0x135A },
    { 0x135D, 0x135F },
    { 0x1369, 0x1371 },
    { 0x1380, 0x138F },
    { 0x13A0, 0x13F5 },
    { 0x13F8, 0x13FD },
    { 0x1401, 0x166C },
    { 0x166F, 0x167F },
    { 0x1681, 0x169A },
    { 0x16A0, 0x16EA },
    { 0x16EE, 0x16F0 },
    { 0x16F1, 0x16F8 },
    { 0x1700, 0x1711 },
    { 0x1712, 0x1714 },
    { 0x1715, 0x1715 },
    { 0x171F, 0x1731 },
    { 0x1732, 0x1733 },
    { 0x1734, 0x1734 },
    { 0x1740, 0x1751 },
    { 0x1752, 0x1753 },
    { 0x1760, 0x176C },
    { 0x176E, 0x1770 },
    { 0x1772, 0x1773 },
    { 0x1780, 0x17B3 },
    { 0x17B4, 0x17B5 },
    { 0x17B6, 0x17B6 },
    { 0x17B7, 0x17BD },
    { 0x17BE, 0x17C5 },
    { 0x17C6, 0x17C6 },
    { 0x17C7, 0x17C8 },
    { 0x17C9, 0x17D3 },
    { 0x17D7, 0x17D7 },
    { 0x17DC, 0x17DC },
    { 0x17DD, 0x17DD },
    { 0x17E0, 0x17E9 },
    { 0x180B, 0x180D },
    { 0x180F, 0x180F },
    { 0x1810, 0x1819 },
    { 0x1820, 0x1842 },
    { 0x1843, 0x1843 },
    { 0x1844, 0x1878 },
    { 0x1880, 0x1884 },
    { 0x1885, 0x1886 },
    { 0x1887, 0x18A8 },
    { 0x18A9, 0x18A9 },
    { 0x18AA, 0x18AA },
    { 0x18B0, 0x18F5 },
    { 0x1900, 0x191E },
    { 0x1920, 0x1922 },
    { 0x1923, 0x1926 },
    { 0x1927, 0x1928 },
    { 0x1929, 0x192B },
    { 0x1930, 0x1931 },
    { 0x1932, 0x1932 },
    { 0x1933, 0x1938 },
    { 0x1939, 0x193B },
    { 0x1946, 0x194F },
    { 0x1950, 0x196D },
    { 0x1970, 0x1974 },
    { 0x1980, 0x19AB },
    { 0x19B0, 0x19C9 },
    { 0x19D0, 0x19D9 },
    { 0x19DA, 0x19DA },
    { 0x1A00, 0x1A16 },
    { 0x1A17, 0x1A18 },
    { 0x1A19, 0x1A1A },
    { 0x1A1B, 0x1A1B },
    { 0x1A20, 0x1A54 },
    { 0x1A55, 0x1A55 },
    { 0x1A56, 0x1A56 },
    { 0x1A57, 0x1A57 },
    { 0x1A58, 0x1A5E },
    { 0x1A60, 0x1A60 },
    { 0x1A61, 0x1A61 },
    { 0x1A62, 0x1A62 },
    { 0x1A63, 0x1A64 },
    { 0x1A65, 0x1A6C },
    { 0x1A6D, 0x1A72 },
    { 0x1A73, 0x1A7C },
    { 0x1A7F, 0x1A7F },
    { 0x1A80, 0x1A89 },
    { 0x1A90, 0x1A99 },
    { 0x1AA7, 0x1AA7 },
    { 0x1AB0, 0x1ABD },
    { 0x1ABF, 0x1ACE },
    { 0x1B00, 0x1B03 },
    { 0x1B04, 0x1B04 },
    { 0x1B05, 0x1B33 },
    { 0x1B34, 0x1B34 },
    { 0x1B35, 0x1B35 },
    { 0x1B36, 0x1B3A },
    { 0x1B3B, 0x1B3B },
    { 0x1B3C, 0x1B3C },
    { 0x1B3D, 0x1B41 },
    { 0x1B42, 0x1B42 },
    { 0x1B43, 0x1B44 },
    { 0x1B45, 0x1B4C },
    { 0x1B50, 0x1B59 },
    { 0x1B6B, 0x1B73 },
    { 0x1B80, 0x1B81 },
    { 0x1B82, 0x1B82 },
    { 0x1B83, 0x1BA0 },
    { 0x1BA1, 0x1BA1 },
    { 0x1BA2, 0x1BA5 },
    { 0x1BA6, 0x1BA7 },
    { 0x1BA8, 0x1BA9 },
    { 0x1BAA, 0x1BAA },
    { 0x1BAB, 0x1BAD },
    { 0x1BAE, 0x1BAF },
    { 0x1BB0, 0x1BB9 },
    { 0x1BBA, 0x1BE5 },
    { 0x1BE6, 0x1BE6 },
    { 0x1BE7, 0x1BE7 },
    { 0x1BE8, 0x1BE9 },
    { 0x1BEA, 0x1BEC },
    { 0x1BED, 0x1BED },
    { 0x1BEE, 0x1BEE },
    { 0x1BEF, 0x1BF1 },
    { 0x1BF2, 0x1BF3 },
    { 0x1C00, 0x1C23 },
    { 0x1C24, 0x1C2B },
    { 0x1C2C, 0x1C33 },
    { 0x1C34, 0x1C35 },
    { 0x1C36, 0x1C37 },
    { 0x1C40, 0x1C49 },
    { 0x1C4D, 0x1C4F },
    { 0x1C50, 0x1C59 },
    { 0x1C5A, 0x1C77 },
    { 0x1C78, 0x1C7D },
    { 0x1C80, 0x1C88 },
    { 0x1C90, 0x1CBA },
    { 0x1CBD, 0x1CBF },
    { 0x1CD0, 0x1CD2 },
    { 0x1CD4, 0x1CE0 },
    { 0x1CE1, 0x1CE1 },
    { 0x1CE2, 0x1CE8 },
    { 0x1CE9, 0x1CEC },
    { 0x1CED, 0x1CED },
    { 0x1CEE, 0x1CF3 },
    { 0x1CF4, 0x1CF4 },
    { 0x1CF5, 0x1CF6 },
    { 0x1CF7, 0x1CF7 },
    { 0x1CF8, 0x1CF9 },
    { 0x1CFA, 0x1CFA },
    { 0x1D00, 0x1D2B },
    { 0x1D2C, 0x1D6A },
    { 0x1D6B, 0x1D77 },
    { 0x1D78, 0x1D78 },
    { 0x1D79, 0x1D9A },
    { 0x1D9B, 0x1DBF },
    { 0x1DC0, 0x1DFF },
    { 0x1E00, 0x1F15 },
    { 0x1F18, 0x1F1D },
    { 0x1F20, 0x1F45 },
    { 0x1F48, 0x1F4D },
    { 0x1F50, 0x1F57 },
    { 0x1F59, 0x1F59 },
    { 0x1F5B, 0x1F5B },
    { 0x1F5D, 0x1F5D },
    { 0x1F5F, 0x1F7D },
    { 0x1F80, 0x1FB4 },
    { 0x1FB6, 0x1FBC },
    { 0x1FBE, 0x1FBE },
    { 0x1FC2, 0x1FC4 },
    { 0x1FC6, 0x1FCC },
    { 0x1FD0, 0x1FD3 },
    { 0x1FD6, 0x1FDB },
    { 0x1FE0, 0x1FEC },
    { 0x1FF2, 0x1FF4 },
    { 0x1FF6, 0x1FFC },
    { 0x203F, 0x2040 },
    { 0x2054, 0x2054 },
    { 0x2071, 0x2071 },
    { 0x207F, 0x207F },
    { 0x2090, 0x209C },
    { 0x20D0, 0x20DC },
    { 0x20E1, 0x20E1 },
    { 0x20E5, 0x20F0 },
    { 0x2102, 0x2102 },
    { 0x2107, 0x2107 },
    { 0x210A, 0x2113 },
    { 0x2115, 0x2115 },
    { 0x2118, 0x2118 },
    { 0x2119, 0x211D },
    { 0x2124, 0x2124 },
    { 0x2126, 0x2126 },
    { 0x2128, 0x2128 },
    { 0x212A, 0x212D },
    { 0x212E, 0x212E },
    { 0x212F, 0x2134 },
    { 0x2135, 0x2138 },
    { 0x2139, 0x2139 },
    { 0x213C, 0x213F },
    { 0x2145, 0x2149 },
    { 0x214E, 0x214E },
    { 0x2160, 0x2182 },
    { 0x2183, 0x2184 },
    { 0x2185, 0x2188 },
    { 0x2C00, 0x2C7B },
    { 0x2C7C, 0x2C7D },
    { 0x2C7E, 0x2CE4 },
    { 0x2CEB, 0x2CEE },
    { 0x2CEF, 0x2CF1 },
    { 0x2CF2, 0x2CF3 },
    { 0x2D00, 0x2D25 },
    { 0x2D27, 0x2D27 },
    { 0x2D2D, 0x2D2D },
    { 0x2D30, 0x2D67 },
    { 0x2D6F, 0x2D6F },
    { 0x2D7F, 0x2D7F },
    { 0x2D80, 0x2D96 },
    { 0x2DA0, 0x2DA6 },
    { 0x2DA8, 0x2DAE },
    { 0x2DB0, 0x2DB6 },
    { 0x2DB8, 0x2DBE },
    { 0x2DC0, 0x2DC6 },
    { 0x2DC8, 0x2DCE },
    { 0x2DD0, 0x2DD6 },
    { 0x2DD8, 0x2DDE },
    { 0x2DE0, 0x2DFF },
    { 0x3005, 0x3005 },
    { 0x3006, 0x3006 },
    { 0x3007, 0x3007 },
    { 0x3021, 0x3029 },
    { 0x302A, 0x302D },
    { 0x302E, 0x302F },
    { 0x3031, 0x3035 },
    { 0x3038, 0x303A },
    { 0x303B, 0x303B },
    { 0x303C, 0x303C },
    { 0x3041, 0x3096 },
    { 0x3099, 0x309A },
    { 0x309D, 0x309E },
    { 0x309F, 0x309F },
    { 0x30A1, 0x30FA },
    { 0x30FC, 0x30FE },
    { 0x30FF, 0x30FF },
    { 0x3105, 0x312F },
    { 0x3131, 0x318E },
    { 0x31A0, 0x31BF },
    { 0x31F0, 0x31FF },
    { 0x3400, 0x4DBF },
    { 0x4E00, 0xA014 },
    { 0xA015, 0xA015 },
    { 0xA016, 0xA48C },
    { 0xA4D0, 0xA4F7 },
    { 0xA4F8, 0xA4FD },
    { 0xA500, 0xA60B },
    { 0xA60C, 0xA60C },
    { 0xA610, 0xA61F },
    { 0xA620, 0xA629 },
    { 0xA62A, 0xA62B },
    { 0xA640, 0xA66D },
    { 0xA66E, 0xA66E },
    { 0xA66F, 0xA66F },
    { 0xA674, 0xA67D },
    { 0xA67F, 0xA67F },
    { 0xA680, 0xA69B },
    { 0xA69C, 0xA69D },
    { 0xA69E, 0xA69F },
    { 0xA6A0, 0xA6E5 },
    { 0xA6E6, 0xA6EF },
    { 0xA6F0, 0xA6F1 },
    { 0xA717, 0xA71F },
    { 0xA722, 0xA76F },
    { 0xA770, 0xA770 },
    { 0xA771, 0xA787 },
    { 0xA788, 0xA788 },
    { 0xA78B, 0xA78E },
    { 0xA78F, 0xA78F },
    { 0xA790, 0xA7CA },
    { 0xA7D0, 0xA7D1 },
    { 0xA7D3, 0xA7D3 },
    { 0xA7D5, 0xA7D9 },
    { 0xA7F2, 0xA7F4 },
    { 0xA7F5, 0xA7F6 },
    { 0xA7F7, 0xA7F7 },
    { 0xA7F8, 0xA7F9 },
    { 0xA7FA, 0xA7FA },
    { 0xA7FB, 0xA801 },
    { 0xA802, 0xA802 },
    { 0xA803, 0xA805 },
    { 0xA806, 0xA806 },
    { 0xA807, 0xA80A },
    { 0xA80B, 0xA80B },
    { 0xA80C, 0xA822 },
    { 0xA823, 0xA824 },
    { 0xA825, 0xA826 },
    { 0xA827, 0xA827 },
    { 0xA82C, 0xA82C },
    { 0xA840, 0xA873 },
    { 0xA880, 0xA881 },
    { 0xA882, 0xA8B3 },
    { 0xA8B4, 0xA8C3 },
    { 0xA8C4, 0xA8C5 },
    { 0xA8D0, 0xA8D9 },
    { 0xA8E0, 0xA8F1 },
    { 0xA8F2, 0xA8F7 },
    { 0xA8FB, 0xA8FB },
    { 0xA8FD, 0xA8FE },
    { 0xA8FF, 0xA8FF },
    { 0xA900, 0xA909 },
    { 0xA90A, 0xA925 },
    { 0xA926, 0xA92D },
    { 0xA930, 0xA946 },
    { 0xA947, 0xA951 },
    { 0xA952, 0xA953 },
    { 0xA960, 0xA97C },
    { 0xA980, 0xA982 },
    { 0xA983, 0xA983 },
    { 0xA984, 0xA9B2 },
    { 0xA9B3, 0xA9B3 },
    { 0xA9B4, 0xA9B5 },
    { 0xA9B6, 0xA9B9 },
    { 0xA9BA, 0xA9BB },
    { 0xA9BC, 0xA9BD },
    { 0xA9BE, 0xA9C0 },
    { 0xA9CF, 0xA9CF },
    { 0xA9D0, 0xA9D9 },
    { 0xA9E0, 0xA9E4 },
    { 0xA9E5, 0xA9E5 },
    { 0xA9E6, 0xA9E6 },
    { 0xA9E7, 0xA9EF },
    { 0xA9F0, 0xA9F9 },
    { 0xA9FA, 0xA9FE },
    { 0xAA00, 0xAA28 },
    { 0xAA29, 0xAA2E },
    { 0xAA2F, 0xAA30 },
    { 0xAA31, 0xAA32 },
    { 0xAA33, 0xAA34 },
    { 0xAA35, 0xAA36 },
    { 0xAA40, 0xAA42 },
    { 0xAA43, 0xAA43 },
    { 0xAA44, 0xAA4B },
    { 0xAA4C, 0xAA4C },
    { 0xAA4D, 0xAA4D },
    { 0xAA50, 0xAA59 },
    { 0xAA60, 0xAA6F },
    { 0xAA70, 0xAA70 },
    { 0xAA71, 0xAA76 },
    { 0xAA7A, 0xAA7A },
    { 0xAA7B, 0xAA7B },
    { 0xAA7C, 0xAA7C },
    { 0xAA7D, 0xAA7D },
    { 0xAA7E, 0xAAAF },
    { 0xAAB0, 0xAAB0 },
    { 0xAAB1, 0xAAB1 },
    { 0xAAB2, 0xAAB4 },
    { 0xAAB5, 0xAAB6 },
    { 0xAAB7, 0xAAB8 },
    { 0xAAB9, 0xAABD },
    { 0xAABE, 0xAABF },
    { 0xAAC0, 0xAAC0 },
    { 0xAAC1, 0xAAC1 },
    { 0xAAC2, 0xAAC2 },
    { 0xAADB, 0xAADC },
    { 0xAADD, 0xAADD },
    { 0xAAE0, 0xAAEA },
    { 0xAAEB, 0xAAEB },
    { 0xAAEC, 0xAAED },
    { 0xAAEE, 0xAAEF },
    { 0xAAF2, 0xAAF2 },
    { 0xAAF3, 0xAAF4 },
    { 0xAAF5, 0xAAF5 },
    { 0xAAF6, 0xAAF6 },
    { 0xAB01, 0xAB06 },
    { 0xAB09, 0xAB0E },
    { 0xAB11, 0xAB16 },
    { 0xAB20, 0xAB26 },
    { 0xAB28, 0xAB2E },
    { 0xAB30, 0xAB5A },
    { 0xAB5C, 0xAB5F },
    { 0xAB60, 0xAB68 },
    { 0xAB69, 0xAB69 },
    { 0xAB70, 0xABBF },
    { 0xABC0, 0xABE2 },
    { 0xABE3, 0xABE4 },
    { 0xABE5, 0xABE5 },
    { 0xABE6, 0xABE7 },
    { 0xABE8, 0xABE8 },
    { 0xABE9, 0xABEA },
    { 0xABEC, 0xABEC },
    { 0xABED, 0xABED },
    { 0xABF0, 0xABF9 },
    { 0xAC00, 0xD7A3 },
    { 0xD7B0, 0xD7C6 },
    { 0xD7CB, 0xD7FB },
    { 0xF900, 0xFA6D },
    { 0xFA70, 0xFAD9 },
    { 0xFB00, 0xFB06 },
    { 0xFB13, 0xFB17 },
    { 0xFB1D, 0xFB1D },
    { 0xFB1E, 0xFB1E },
    { 0xFB1F, 0xFB28 },
    { 0xFB2A, 0xFB36 },
    { 0xFB38, 0xFB3C },
    { 0xFB3E, 0xFB3E },
    { 0xFB40, 0xFB41 },
    { 0xFB43, 0xFB44 },
    { 0xFB46, 0xFBB1 },
    { 0xFBD3, 0xFC5D },
    { 0xFC64, 0xFD3D },
    { 0xFD50, 0xFD8F },
    { 0xFD92, 0xFDC7 },
    { 0xFDF0, 0xFDF9 },
    { 0xFE00, 0xFE0F },
    { 0xFE20, 0xFE2F },
    { 0xFE33, 0xFE34 },
    { 0xFE4D, 0xFE4F },
    { 0xFE71, 0xFE71 },
    { 0xFE73, 0xFE73 },
    { 0xFE77, 0xFE77 },
    { 0xFE79, 0xFE79 },
    { 0xFE7B, 0xFE7B },
    { 0xFE7D, 0xFE7D },
    { 0xFE7F, 0xFEFC },
    { 0xFF10, 0xFF19 },
    { 0xFF21, 0xFF3A },
    { 0xFF3F, 0xFF3F },
    { 0xFF41, 0xFF5A },
    { 0xFF66, 0xFF6F },
    { 0xFF70, 0xFF70 },
    { 0xFF71, 0xFF9D },
    { 0xFF9E, 0xFF9F },
    { 0xFFA0, 0xFFBE },
    { 0xFFC2, 0xFFC7 },
    { 0xFFCA, 0xFFCF },
    { 0xFFD2, 0xFFD7 },
    { 0xFFDA, 0xFFDC },
    { 0x10000, 0x1000B },
    { 0x1000D, 0x10026 },
    { 0x10028, 0x1003A },
    { 0x1003C, 0x1003D },
    { 0x1003F, 0x1004D },
    { 0x10050, 0x1005D },
    { 0x10080, 0x100FA },
    { 0x10140, 0x10174 },
    { 0x101FD, 0x101FD },
    { 0x10280, 0x1029C },
    { 0x102A0, 0x102D0 },
    { 0x102E0, 0x102E0 },
    { 0x10300, 0x1031F },
    { 0x1032D, 0x10340 },
    { 0x10341, 0x10341 },
    { 0x10342, 0x10349 },
    { 0x1034A, 0x1034A },
    { 0x10350, 0x10375 },
    { 0x10376, 0x1037A },
    { 0x10380, 0x1039D },
    { 0x103A0, 0x103C3 },
    { 0x103C8, 0x103CF },
    { 0x103D1, 0x103D5 },
    { 0x10400, 0x1044F },
    { 0x10450, 0x1049D },
    { 0x104A0, 0x104A9 },
    { 0x104B0, 0x104D3 },
    { 0x104D8, 0x104FB },
    { 0x10500, 0x10527 },
    { 0x10530, 0x10563 },
    { 0x10570, 0x1057A },
    { 0x1057C, 0x1058A },
    { 0x1058C, 0x10592 },
    { 0x10594, 0x10595 },
    { 0x10597, 0x105A1 },
    { 0x105A3, 0x105B1 },
    { 0x105B3, 0x105B9 },
    { 0x105BB, 0x105BC },
    { 0x10600, 0x10736 },
    { 0x10740, 0x10755 },
    { 0x10760, 0x10767 },
    { 0x10780, 0x10785 },
    { 0x10787, 0x107B0 },
    { 0x107B2, 0x107BA },
    { 0x10800, 0x10805 },
    { 0x10808, 0x10808 },
    { 0x1080A, 0x10835 },
    { 0x10837, 0x10838 },
    { 0x1083C, 0x1083C },
    { 0x1083F, 0x10855 },
    { 0x10860, 0x10876 },
    { 0x10880, 0x1089E },
    { 0x108E0, 0x108F2 },
    { 0x108F4, 0x108F5 },
    { 0x10900, 0x10915 },
    { 0x10920, 0x10939 },
    { 0x10980, 0x109B7 },
    { 0x109BE, 0x109BF },
    { 0x10A00, 0x10A00 },
    { 0x10A01, 0x10A03 },
    { 0x10A05, 0x10A06 },
    { 0x10A0C, 0x10A0F },
    { 0x10A10, 0x10A13 },
    { 0x10A15, 0x10A17 },
    { 0x10A19, 0x10A35 },
    { 0x10A38, 0x10A3A },
    { 0x10A3F, 0x10A3F },
    { 0x10A60, 0x10A7C },
    { 0x10A80, 0x10A9C },
    { 0x10AC0, 0x10AC7 },
    { 0x10AC9, 0x10AE4 },
    { 0x10AE5, 0x10AE6 },
    { 0x10B00, 0x10B35 },
    { 0x10B40, 0x10B55 },
    { 0x10B60, 0x10B72 },
    { 0x10B80, 0x10B91 },
    { 0x10C00, 0x10C48 },
    { 0x10C80, 0x10CB2 },
    { 0x10CC0, 0x10CF2 },
    { 0x10D00, 0x10D23 },
    { 0x10D24, 0x10D27 },
    { 0x10D30, 0x10D39 },
    { 0x10E80, 0x10EA9 },
    { 0x10EAB, 0x10EAC },
    { 0x10EB0, 0x10EB1 },
    { 0x10F00, 0x10F1C },
    { 0x10F27, 0x10F27 },
    { 0x10F30, 0x10F45 },
    { 0x10F46, 0x10F50 },
    { 0x10F70, 0x10F81 },
    { 0x10F82, 0x10F85 },
    { 0x10FB0, 0x10FC4 },
    { 0x10FE0, 0x10FF6 },
    { 0x11000, 0x11000 },
    { 0x11001, 0x11001 },
    { 0x11002, 0x11002 },
    { 0x11003, 0x11037 },
    { 0x11038, 0x11046 },
    { 0x11066, 0x1106F },
    { 0x11070, 0x11070 },
    { 0x11071, 0x11072 },
    { 0x11073, 0x11074 },
    { 0x11075, 0x11075 },
    { 0x1107F, 0x11081 },
    { 0x11082, 0x11082 },
    { 0x11083, 0x110AF },
    { 0x110B0, 0x110B2 },
    { 0x110B3, 0x110B6 },
    { 0x110B7, 0x110B8 },
    { 0x110B9, 0x110BA },
    { 0x110C2, 0x110C2 },
    { 0x110D0, 0x110E8 },
    { 0x110F0, 0x110F9 },
    { 0x11100, 0x11102 },
    { 0x11103, 0x11126 },
    { 0x11127, 0x1112B },
    { 0x1112C, 0x1112C },
    { 0x1112D, 0x11134 },
    { 0x11136, 0x1113F },
    { 0x11144, 0x11144 },
    { 0x11145, 0x11146 },
    { 0x11147, 0x11147 },
    { 0x11150, 0x11172 },
    { 0x11173, 0x11173 },
    { 0x11176, 0x11176 },
    { 0x11180, 0x11181 },
    { 0x11182, 0x11182 },
    { 0x11183, 0x111B2 },
    { 0x111B3, 0x111B5 },
    { 0x111B6, 0x111BE },
    { 0x111BF, 0x111C0 },
    { 0x111C1, 0x111C4 },
    { 0x111C9, 0x111CC },
    { 0x111CE, 0x111CE },
    { 0x111CF, 0x111CF },
    { 0x111D0, 0x111D9 },
    { 0x111DA, 0x111DA },
    { 0x111DC, 0x111DC },
    { 0x11200, 0x11211 },
    { 0x11213, 0x1122B },
    { 0x1122C, 0x1122E },
    { 0x1122F, 0x11231 },
    { 0x11232, 0x11233 },
    { 0x11234, 0x11234 },
    { 0x11235, 0x11235 },
    { 0x11236, 0x11237 },
    { 0x1123E, 0x1123E },
    { 0x11280, 0x11286 },
    { 0x11288, 0x11288 },
    { 0x1128A, 0x1128D },
    { 0x1128F, 0x1129D },
    { 0x1129F, 0x112A8 },
    { 0x112B0, 0x112DE },
    { 0x112DF, 0x112DF },
    { 0x112E0, 0x112E2 },
    { 0x112E3, 0x112EA },
    { 0x112F0, 0x112F9 },
    { 0x11300, 0x11301 },
    { 0x11302, 0x11303 },
    { 0x11305, 0x1130C },
    { 0x1130F, 0x11310 },
    { 0x11313, 0x11328 },
    { 0x1132A, 0x11330 },
    { 0x11332, 0x11333 },
    { 0x11335, 0x11339 },
    { 0x1133B, 0x1133C },
    { 0x1133D, 0x1133D },
    { 0x1133E, 0x1133F },
    { 0x11340, 0x11340 },
    { 0x11341, 0x11344 },
    { 0x11347, 0x11348 },
    { 0x1134B, 0x1134D },
    { 0x11350, 0x11350 },
    { 0x11357, 0x11357 },
    { 0x1135D, 0x11361 },
    { 0x11362, 0x11363 },
    { 0x11366, 0x1136C },
    { 0x11370, 0x11374 },
    { 0x11400, 0x11434 },
    { 0x11435, 0x11437 },
    { 0x11438, 0x1143F },
    { 0x11440, 0x11441 },
    { 0x11442, 0x11444 },
    { 0x11445, 0x11445 },
    { 0x11446, 0x11446 },
    { 0x11447, 0x1144A },
    { 0x11450, 0x11459 },
    { 0x1145E, 0x1145E },
    { 0x1145F, 0x11461 },
    { 0x11480, 0x114AF },
    { 0x114B0, 0x114B2 },
    { 0x114B3, 0x114B8 },
    { 0x114B9, 0x114B9 },
    { 0x114BA, 0x114BA },
    { 0x114BB, 0x114BE },
    { 0x114BF, 0x114C0 },
    { 0x114C1, 0x114C1 },
    { 0x114C2, 0x114C3 },
    { 0x114C4, 0x114C5 },
    { 0x114C7, 0x114C7 },
    { 0x114D0, 0x114D9 },
    { 0x11580, 0x115AE },
    { 0x115AF, 0x115B1 },
    { 0x115B2, 0x115B5 },
    { 0x115B8, 0x115BB },
    { 0x115BC, 0x115BD },
    { 0x115BE, 0x115BE },
    { 0x115BF, 0x115C0 },
    { 0x115D8, 0x115DB },
    { 0x115DC, 0x115DD },
    { 0x11600, 0x1162F },
    { 0x11630, 0x11632 },
    { 0x11633, 0x1163A },
    { 0x1163B, 0x1163C },
    { 0x1163D, 0x1163D },
    { 0x1163E, 0x1163E },
    { 0x1163F, 0x11640 },
    { 0x11644, 0x11644 },
    { 0x11650, 0x11659 },
    { 0x11680, 0x116AA },
    { 0x116AB, 0x116AB },
    { 0x116AC, 0x116AC },
    { 0x116AD, 0x116AD },
    { 0x116AE, 0x116AF },
    { 0x116B0, 0x116B5 },
    { 0x116B6, 0x116B6 },
    { 0x116B7, 0x116B7 },
    { 0x116B8, 0x116B8 },
    { 0x116C0, 0x116C9 },
    { 0x11700, 0x1171A },
    { 0x1171D, 0x1171F },
    { 0x11720, 0x11721 },
    { 0x11722, 0x11725 },
    { 0x11726, 0x11726 },
    { 0x11727, 0x1172B },
    { 0x11730, 0x11739 },
    { 0x11740, 0x11746 },
    { 0x11800, 0x1182B },
    { 0x1182C, 0x1182E },
    { 0x1182F, 0x11837 },
    { 0x11838, 0x11838 },
    { 0x11839, 0x1183A },
    { 0x118A0, 0x118DF },
    { 0x118E0, 0x118E9 },
    { 0x118FF, 0x11906 },
    { 0x11909, 0x11909 },
    { 0x1190C, 0x11913 },
    { 0x11915, 0x11916 },
    { 0x11918, 0x1192F },
    { 0x11930, 0x11935 },
    { 0x11937, 0x11938 },
    { 0x1193B, 0x1193C },
    { 0x1193D, 0x1193D },
    { 0x1193E, 0x1193E },
    { 0x1193F, 0x1193F },
    { 0x11940, 0x11940 },
    { 0x11941, 0x11941 },
    { 0x11942, 0x11942 },
    { 0x11943, 0x11943 },
    { 0x11950, 0x11959 },
    { 0x119A0, 0x119A7 },
    { 0x119AA, 0x119D0 },
    { 0x119D1, 0x119D3 },
    { 0x119D4, 0x119D7 },
    { 0x119DA, 0x119DB },
    { 0x119DC, 0x119DF },
    { 0x119E0, 0x119E0 },
    { 0x119E1, 0x119E1 },
    { 0x119E3, 0x119E3 },
    { 0x119E4, 0x119E4 },
    { 0x11A00, 0x11A00 },
    { 0x11A01, 0x11A0A },
    { 0x11A0B, 0x11A32 },
    { 0x11A33, 0x11A38 },
    { 0x11A39, 0x11A39 },
    { 0x11A3A, 0x11A3A },
    { 0x11A3B, 0x11A3E },
    { 0x11A47, 0x11A47 },
    { 0x11A50, 0x11A50 },
    { 0x11A51, 0x11A56 },
    { 0x11A57, 0x11A58 },
    { 0x11A59, 0x11A5B },
    { 0x11A5C, 0x11A89 },
    { 0x11A8A, 0x11A96 },
    { 0x11A97, 0x11A97 },
    { 0x11A98, 0x11A99 },
    { 0x11A9D, 0x11A9D },
    { 0x11AB0, 0x11AF8 },
    { 0x11C00, 0x11C08 },
    { 0x11C0A, 0x11C2E },
    { 0x11C2F, 0x11C2F },
    { 0x11C30, 0x11C36 },
    { 0x11C38, 0x11C3D },
    { 0x11C3E, 0x11C3E },
    { 0x11C3F, 0x11C3F },
    { 0x11C40, 0x11C40 },
    { 0x11C50, 0x11C59 },
    { 0x11C72, 0x11C8F },
    { 0x11C92, 0x11CA7 },
    { 0x11CA9, 0x11CA9 },
    { 0x11CAA, 0x11CB0 },
    { 0x11CB1, 0x11CB1 },
    { 0x11CB2, 0x11CB3 },
    { 0x11CB4, 0x11CB4 },
    { 0x11CB5, 0x11CB6 },
    { 0x11D00, 0x11D06 },
    { 0x11D08, 0x11D09 },
    { 0x11D0B, 0x11D30 },
    { 0x11D31, 0x11D36 },
    { 0x11D3A, 0x11D3A },
    { 0x11D3C, 0x11D3D },
    { 0x11D3F, 0x11D45 },
    { 0x11D46, 0x11D46 },
    { 0x11D47, 0x11D47 },
    { 0x11D50, 0x11D59 },
    { 0x11D60, 0x11D65 },
    { 0x11D67, 0x11D68 },
    { 0x11D6A, 0x11D89 },
    { 0x11D8A, 0x11D8E },
    { 0x11D90, 0x11D91 },
    { 0x11D93, 0x11D94 },
    { 0x11D95, 0x11D95 },
    { 0x11D96, 0x11D96 },
    { 0x11D97, 0x11D97 },
    { 0x11D98, 0x11D98 },
    { 0x11DA0, 0x11DA9 },
    { 0x11EE0, 0x11EF2 },
    { 0x11EF3, 0x11EF4 },
    { 0x11EF5, 0x11EF6 },
    { 0x11FB0, 0x11FB0 },
    { 0x12000, 0x12399 },
    { 0x12400, 0x1246E },
    { 0x12480, 0x12543 },
    { 0x12F90, 0x12FF0 },
    { 0x13000, 0x1342E },
    { 0x14400, 0x14646 },
    { 0x16800, 0x16A38 },
    { 0x16A40, 0x16A5E },
    { 0x16A60, 0x16A69 },
    { 0x16A70, 0x16ABE },
    { 0x16AC0, 0x16AC9 },
    { 0x16AD0, 0x16AED },
    { 0x16AF0, 0x16AF4 },
    { 0x16B00, 0x16B2F },
    { 0x16B30, 0x16B36 },
    { 0x16B40, 0x16B43 },
    { 0x16B50, 0x16B59 },
    { 0x16B63, 0x16B77 },
    { 0x16B7D, 0x16B8F },
    { 0x16E40, 0x16E7F },
    { 0x16F00, 0x16F4A },
    { 0x16F4F, 0x16F4F },
    { 0x16F50, 0x16F50 },
    { 0x16F51, 0x16F87 },
    { 0x16F8F, 0x16F92 },
    { 0x16F93, 0x16F9F },
    { 0x16FE0, 0x16FE1 },
    { 0x16FE3, 0x16FE3 },
    { 0x16FE4, 0x16FE4 },
    { 0x16FF0, 0x16FF1 },
    { 0x17000, 0x187F7 },
    { 0x18800, 0x18CD5 },
    { 0x18D00, 0x18D08 },
    { 0x1AFF0, 0x1AFF3 },
    { 0x1AFF5, 0x1AFFB },
    { 0x1AFFD, 0x1AFFE },
    { 0x1B000, 0x1B122 },
    { 0x1B150, 0x1B152 },
    { 0x1B164, 0x1B167 },
    { 0x1B170, 0x1B2FB },
    { 0x1BC00, 0x1BC6A },
    { 0x1BC70, 0x1BC7C },
    { 0x1BC80, 0x1BC88 },
    { 0x1BC90, 0x1BC99 },
    { 0x1BC9D, 0x1BC9E },
    { 0x1CF00, 0x1CF2D },
    { 0x1CF30, 0x1CF46 },
    { 0x1D165, 0x1D166 },
    { 0x1D167, 0x1D169 },
    { 0x1D16D, 0x1D172 },
    { 0x1D17B, 0x1D182 },
    { 0x1D185, 0x1D18B },
    { 0x1D1AA, 0x1D1AD },
    { 0x1D242, 0x1D244 },
    { 0x1D400, 0x1D454 },
    { 0x1D456, 0x1D49C },
    { 0x1D49E, 0x1D49F },
    { 0x1D4A2, 0x1D4A2 },
    { 0x1D4A5, 0x1D4A6 },
    { 0x1D4A9, 0x1D4AC },
    { 0x1D4AE, 0x1D4B9 },
    { 0x1D4BB, 0x1D4BB },
    { 0x1D4BD, 0x1D4C3 },
    { 0x1D4C5, 0x1D505 },
    { 0x1D507, 0x1D50A },
    { 0x1D50D, 0x1D514 },
    { 0x1D516, 0x1D51C },
    { 0x1D51E, 0x1D539 },
    { 0x1D53B, 0x1D53E },
    { 0x1D540, 0x1D544 },
    { 0x1D546, 0x1D546 },
    { 0x1D54A, 0x1D550 },
    { 0x1D552, 0x1D6A5 },
    { 0x1D6A8, 0x1D6C0 },
    { 0x1D6C2, 0x1D6DA },
    { 0x1D6DC, 0x1D6FA },
    { 0x1D6FC, 0x1D714 },
    { 0x1D716, 0x1D734 },
    { 0x1D736, 0x1D74E },
    { 0x1D750, 0x1D76E },
    { 0x1D770, 0x1D788 },
    { 0x1D78A, 0x1D7A8 },
    { 0x1D7AA, 0x1D7C2 },
    { 0x1D7C4, 0x1D7CB },
    { 0x1D7CE, 0x1D7FF },
    { 0x1DA00, 0x1DA36 },
    { 0x1DA3B, 0x1DA6C },
    { 0x1DA75, 0x1DA75 },
    { 0x1DA84, 0x1DA84 },
    { 0x1DA9B, 0x1DA9F },
    { 0x1DAA1, 0x1DAAF },
    { 0x1DF00, 0x1DF09 },
    { 0x1DF0A, 0x1DF0A },
    { 0x1DF0B, 0x1DF1E },
    { 0x1E000, 0x1E006 },
    { 0x1E008, 0x1E018 },
    { 0x1E01B, 0x1E021 },
    { 0x1E023, 0x1E024 },
    { 0x1E026, 0x1E02A },
    { 0x1E100, 0x1E12C },
    { 0x1E130, 0x1E136 },
    { 0x1E137, 0x1E13D },
    { 0x1E140, 0x1E149 },
    { 0x1E14E, 0x1E14E },
    { 0x1E290, 0x1E2AD },
    { 0x1E2AE, 0x1E2AE },
    { 0x1E2C0, 0x1E2EB },
    { 0x1E2EC, 0x1E2EF },
    { 0x1E2F0, 0x1E2F9 },
    { 0x1E7E0, 0x1E7E6 },
    { 0x1E7E8, 0x1E7EB },
    { 0x1E7ED, 0x1E7EE },
    { 0x1E7F0, 0x1E7FE },
    { 0x1E800, 0x1E8C4 },
    { 0x1E8D0, 0x1E8D6 },
    { 0x1E900, 0x1E943 },
    { 0x1E944, 0x1E94A },
    { 0x1E94B, 0x1E94B },
    { 0x1E950, 0x1E959 },
    { 0x1EE00, 0x1EE03 },
    { 0x1EE05, 0x1EE1F },
    { 0x1EE21, 0x1EE22 },
    { 0x1EE24, 0x1EE24 },
    { 0x1EE27, 0x1EE27 },
    { 0x1EE29, 0x1EE32 },
    { 0x1EE34, 0x1EE37 },
    { 0x1EE39, 0x1EE39 },
    { 0x1EE3B, 0x1EE3B },
    { 0x1EE42, 0x1EE42 },
    { 0x1EE47, 0x1EE47 },
    { 0x1EE49, 0x1EE49 },
    { 0x1EE4B, 0x1EE4B },
    { 0x1EE4D, 0x1EE4F },
    { 0x1EE51, 0x1EE52 },
    { 0x1EE54, 0x1EE54 },
    { 0x1EE57, 0x1EE57 },
    { 0x1EE59, 0x1EE59 },
    { 0x1EE5B, 0x1EE5B },
    { 0x1EE5D, 0x1EE5D },
    { 0x1EE5F, 0x1EE5F },
    { 0x1EE61, 0x1EE62 },
    { 0x1EE64, 0x1EE64 },
    { 0x1EE67, 0x1EE6A },
    { 0x1EE6C, 0x1EE72 },
    { 0x1EE74, 0x1EE77 },
    { 0x1EE79, 0x1EE7C },
    { 0x1EE7E, 0x1EE7E },
    { 0x1EE80, 0x1EE89 },
    { 0x1EE8B, 0x1EE9B },
    { 0x1EEA1, 0x1EEA3 },
    { 0x1EEA5, 0x1EEA9 },
    { 0x1EEAB, 0x1EEBB },
    { 0x1FBF0, 0x1FBF9 },
    { 0x20000, 0x2A6DF },
    { 0x2A700, 0x2B738 },
    { 0x2B740, 0x2B81D },
    { 0x2B820, 0x2CEA1 },
    { 0x2CEB0, 0x2EBE0 },
    { 0x2F800, 0x2FA1D },
    { 0x30000, 0x3134A },
    { 0xE0100, 0xE01EF },
};

template <class RangeContainer>
constexpr
bool binary_range_search(std::uint32_t n, const RangeContainer& table, size_t table_size) {
    int l = 0;
    int h = static_cast<int>(table_size);
    int m = 0;
    while (l <= h) {
        m = (l + h) / 2;
        if (n < table[m][0]) {
            h = m - 1;
        } else if (n > table[m][1]) {
            l = m + 1;
        } else {
            break;
        }
    }
    return l <= h;
}

constexpr inline
bool is_xid_start(pp::Scanner::Char32 c) {
    return binary_range_search(to_int(c), xid_start_table, size(xid_start_table));
}

constexpr inline
bool is_xid_continue(pp::Scanner::Char32 c) {
    return binary_range_search(to_int(c), xid_continue_table, size(xid_continue_table));
}

constexpr inline
bool is_valid_ucn(std::uint32_t n) {
    if ((n < 0xA0) && (n != 0x24 && n != 0x40 && n != 0x60)) {
        return false;
    }
    if (0xD800 <= n && n <= 0xDFFF) {
        return false;
    }
    if (0x10FFFF > n) {
        return false;
    }

    return true;
}

uint32_t parse_ucn_digits(const char* first, const char* last) {
    uint32_t n;
    auto r = from_chars(first, last, n, 16);
    if (r.ec != errc{}) {
        raise_generic_error(__func__, r.ec);
    }

    return n;
}

// UCNの受理時に識別子に使える値かどうかをチェックするようにしたので、とりあえずこれは
// 使わないようにした。その内に消すかもしれない。
//bool contains_invalid_ucn_for_identifier(const std::string& s) {
//    constexpr string::size_type kUcnDigitStart = 2;
//    constexpr string::size_type kUcn16Digits = 4;
//    constexpr string::size_type kUcn32Digits = 8;
//
//    string::size_type i = 0;
//    string::size_type digits = 0;
//    bool ucn;
//    if (s[0] != '\\') {
//        ++i;
//    } else {
//        switch (s[1]) {
//        case 'u':
//            digits = kUcn16Digits;
//            ucn = true;
//            break;
//
//        case 'U':
//            digits = kUcn32Digits;
//            ucn = true;
//            break;
//
//        default:
//            assert(false);
//            ucn = false;
//            break;
//        }
//
//        if (ucn) {
//            uint32_t n = parse_ucn_digits(&s[kUcnDigitStart], &s[kUcnDigitStart + digits]);
//            if (!is_xid_start(to_c32(n))) {
//                return true;
//            }
//            i += kUcnDigitStart + digits;
//        } else {
//            ++i;
//        }
//    }
//
//    while (i < s.length()) {
//        if (s[i] != '\\') {
//            ++i;
//        } else {
//            switch (s[i + 1]) {
//            case 'u':
//                digits = kUcn16Digits;
//                ucn = true;
//                break;
//
//            case 'U':
//                digits = kUcn32Digits;
//                ucn = true;
//                break;
//
//            default:
//                ucn = false;
//                break;
//            }
//
//            if (ucn) {
//                uint32_t n = parse_ucn_digits(&s[i + kUcnDigitStart], &s[i + kUcnDigitStart + digits]);
//                if (!is_xid_continue(to_c32(n))) {
//                    return true;
//                }
//                i += kUcnDigitStart + digits;
//            } else {
//                ++i;
//            }
//        }
//    }
//
//    return false;
//}

constexpr inline
char to_c(int c) {
    return static_cast<char>(c);
}

std::string& to_upper_string(std::string& s, std::size_t pos = 0) {
    for (size_t i = pos; i < s.length(); i++) {
        if ('a' <= s[i] && s[i] <= 'z') {
            s[i] = 'A' + (s[i] - 'a');
        }
    }
    return s;
}

}   // anonymous namespace

namespace pp {

Scanner::Scanner(std::istream& input, bool trigraph, Diagnostics& diag, SourceFileStack& sources)
    : input_(input)
    , diag_(diag)
    , sources_(sources)
    , buf_()
    , buf_i_()
    , line_number_()
    , trigraph_(trigraph)
    , c_()
    , cseq_()
    , state_(ScannerState::kInitial)
    , return_state_(ScannerState::kInitial)
    , hint_(ScannerHint::kInitial)
    , type_(TokenType::kNull)
    , match_()
    , buf_i_mark_()
    , ucn_digit_start_()
    , eof_() {
    //  申し訳程度
    static constexpr char kUtf8Bom[] = "\xef\xbb\xbf";
    for (auto b : kUtf8Bom) {
        int p = input.peek();
        int q = (b & 0xff);
        if (p != q) {
            break;
        }
        input.get();
    }

    c_ = get();
}

Scanner::~Scanner() {
}

Token Scanner::next_token() {
    if (eof()) {
        return Token("", TokenType::kEndOfFile);
    }

    clear_mark();
    bool error = false;
    uint32_t line_number = line_number_;
    uint32_t column = buf_i_;

    state_ = ScannerState::kInitial;
    return_state_ = ScannerState::kInitial;
    cseq_.clear();
    type_ = TokenType::kNull;
    match_ = false;

    while (!match_ && !error) {
        switch (state_) {
        case ScannerState::kInitial: {
            const int k = char_kind(c_);
            switch (k) {
            case kWsChar:
                if (c_ == to_c32('\n')) {
                    consume(c_);
                    finish(TokenType::kNewLine);
                } else if (c_ == to_c32('\r')) {
                    transit(ScannerState::kLineBreak, c_);
                } else {
                    transit(ScannerState::kWhiteSpaces, c_);
                }
                break;

            case kDigit:
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188, c_);
                break;

            case kLetter:
                if (c_ == to_c32('L')) {
                    transit(ScannerState::kState232_225_244_264_249_234_269, c_);
                } else if (c_ == to_c32('U')) {
                    transit(ScannerState::kState242_262_232_249_234_269_223, c_);
                } else if (c_ == to_c32('u')) {
                    transit(ScannerState::kState256_257_260_249_232_234_236_269_237_240_217_218_221, c_);
                } else {
                    transit(ScannerState::kState232_234_227, c_);
                }
                break;

            default:
                switch (to_int(c_)) {
                case '_':
                    transit(ScannerState::kState232_234_227, c_);
                    break;

                case  '\\':
                    transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_234_227);
                    break;

                case '!':
                    transit(ScannerState::kState68_69_38, c_);
                    break;

                case '"':
                    if (hint_ == ScannerHint::kHeaderName) {
                        transit(ScannerState::kStateHeaderNameInitial, c_);
                    } else {
                        transit(ScannerState::kState273_274_270_271, c_);
                    }
                    break;

                case '#':
                    transit(ScannerState::kState148_150_151, c_);
                    break;

                case '%':
                    transit(ScannerState::kState166_167_170_42_171_174_110_175_111, c_);
                    break;

                case '&':
                    transit(ScannerState::kState28_134_135_76_77, c_);
                    break;

                case '\'':
                    transit(ScannerState::kState250_251, c_);
                    break;

                case '(':
                    transit(ScannerState::kState6, c_);
                    break;

                case ')':
                    transit(ScannerState::kState8, c_);
                    break;

                case '*':
                    transit(ScannerState::kState103_102_30, c_);
                    break;

                case '+':
                    transit(ScannerState::kState32_114_115_20_21, c_);
                    break;

                case ',':
                    transit(ScannerState::kState146, c_);
                    break;

                case '-':
                    transit(ScannerState::kState16_17_34_118_119_24_25, c_);
                    break;

                case '.':
                    transit(ScannerState::kState182_184_94_14_95, c_);
                    break;

                case '/':
                    transit(ScannerState::kState40_106_107, c_);
                    break;

                case ':':
                    transit(ScannerState::kState86_88_89_158_159, c_);
                    break;

                case ';':
                    transit(ScannerState::kState92, c_);
                    break;

                case '<':
                    if (hint_ == ScannerHint::kHeaderName) {
                        transit(ScannerState::kStateHeaderNameInitial, c_);
                    } else {
                        transit(ScannerState::kState162_163_122_123_44_45_52_56_57_154_155, c_);
                    }
                    break;

                case '=':
                    transit(ScannerState::kState64_65_100, c_);
                    break;

                case '>':
                    transit(ScannerState::kState128_129_48_49_54_60_61, c_);
                    break;

                case '?':
                    transit(ScannerState::kState84, c_);
                    break;

                //case 'D':
                //    transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188, c_);
                //    break;

                //case 'L':
                //    transit(ScannerState::kState232_225_244_264_249_234_269, c_);
                //    break;

                //case 'S':
                //    transit(ScannerState::kState232_234_227, c_);
                //    break;

                //case 'U':
                //    transit(ScannerState::kState242_262_232_249_234_269_223, c_);
                //    break;

                case '[':
                    transit(ScannerState::kState2, c_);
                    break;

                case ']':
                    transit(ScannerState::kState4, c_);
                    break;

                case '^':
                    transit(ScannerState::kState72_138_139, c_);
                    break;

                //case 'u':
                //    transit(ScannerState::kState256_257_260_249_232_234_236_269_237_240_217_218_221, c_);
                //    break;

                case '{':
                    transit(ScannerState::kState10, c_);
                    break;

                case '|':
                    transit(ScannerState::kState80_81_74_142_143, c_);
                    break;

                case '}':
                    transit(ScannerState::kState12, c_);
                    break;

                case '~':
                    transit(ScannerState::kState36, c_);
                    break;

                default:
                    if (is_xid_start(c_)) {
                        transit(ScannerState::kState232_234_227, c_);
                    } else {
                        consume(c_);
                        finish(TokenType::kNonWhiteSpaceCharacter);
                    }
                    break;
                }
            }

            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D"
            }
            break;
        }
        case ScannerState::kState86_88_89_158_159: {
            mark();
            if (c_ == to_c32(':')) {
                transit(ScannerState::kState90, c_);
            } else if (c_ == to_c32('>')) {
                transit(ScannerState::kState160, c_);
            } else {
                finish(TokenType::kPunctuator); // ":"
            }
            break;
        }
        case ScannerState::kState256_257_260_249_232_234_236_269_237_240_217_218_221: {
            mark();
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == to_c32('\'')) {
                transit(ScannerState::kState250_251, c_);
            } else if (c_ == to_c32('8')) {
                transit(ScannerState::kState258_232_249_234_219_269_238, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "u"
            }
            break;
        }
        case ScannerState::kState80_81_74_142_143: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState144, c_);
            } else if (c_ == to_c32('|')) {
                transit(ScannerState::kState82, c_);
            } else {
                finish(TokenType::kPunctuator); // "|"
            }
            break;
        }
        case ScannerState::kState10: {
            mark();
            finish(TokenType::kLeftBrace); // "{"
            break;
        }
        case ScannerState::kState64_65_100: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState66, c_);
            } else {
                finish(TokenType::kPunctuator); // "="
            }
            break;
        }
        case ScannerState::kState160: {
            mark();
            finish(TokenType::kRightBracket);    // ":>"
            break;
        }
        case ScannerState::kState36: {
            mark();
            finish(TokenType::kPunctuator); // "~"
            break;
        }
        case ScannerState::kState207_199: {
            if (c_ == to_c32('+') || c_ == to_c32('-')) {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState193_194_189_190: {
            if (is_digit(c_)) {  // 'D'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_188_191, c_);
            } else if (is_nondigit(c_)) {  // 'N'
                transit(ScannerState::kState192_195_196_198_200_202_204_205_206_209_211_212_213_214_186_188, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState68_69_38: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState70, c_);
            } else {
                finish(TokenType::kPunctuator); // "!"
            }
            break;
        }
        case ScannerState::kState103_102_30: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState104, c_);
            } else {
                finish(TokenType::kPunctuator); // "*"
            }
            break;
        }
        case ScannerState::kState90: {
            mark();
            finish(TokenType::kPunctuator); // "::"
            break;
        }
        case ScannerState::kState197_207: {
            if (c_ == to_c32('+') || c_ == to_c32('-')) {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState28_134_135_76_77: {
            mark();
            if (c_ == to_c32('&')) {
                transit(ScannerState::kState78, c_);
            } else if (c_ == to_c32('=')) {
                transit(ScannerState::kState136, c_);
            } else {
                finish(TokenType::kPunctuator); // "&"
            }
            break;
        }
        case ScannerState::kState258_232_249_234_219_269_238: {
            mark();
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == to_c32('\'')) {
                transit(ScannerState::kState250_251, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "u8"
            }
            break;
        }
        case ScannerState::kState16_17_34_118_119_24_25: {
            mark();
            if (c_ == to_c32('-')) {
                transit(ScannerState::kState26, c_);
            } else if (c_ == to_c32('=')) {
                transit(ScannerState::kState120, c_);
            } else if (c_ == to_c32('>')) {
                transit(ScannerState::kState18, c_);
            } else {
                finish(TokenType::kPunctuator); // "-"
            }
            break;
        }
        case ScannerState::kState8: {
            mark();
            finish(TokenType::kRightParenthesis); // ")"
            break;
        }
        case ScannerState::kState82: {
            mark();
            finish(TokenType::kPunctuator); // "||"
            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_188_191: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D\'D"
            }
            break;
        }
        case ScannerState::kState92: {
            mark();
            finish(TokenType::kPunctuator); // ";"
            break;
        }
        case ScannerState::kState166_167_170_42_171_174_110_175_111: {
            mark();
            if (c_ == to_c32(':')) {
                transit(ScannerState::kState176_177_172, c_);
            } else if (c_ == to_c32('=')) {
                transit(ScannerState::kState112, c_);
            } else if (c_ == to_c32('>')) {
                transit(ScannerState::kState168, c_);
            } else {
                finish(TokenType::kPunctuator); // "%"
            }
            break;
        }
        case ScannerState::kState201_207: {
            if (c_ == to_c32('+') || c_ == to_c32('-')) {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState273_274_270_271: {
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState275, c_);
            } else if (c_ == to_c32('\\')) {    // 's'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState272_274_271);
            } else {    // 's'
                if (is_nl(c_) || eof()) {
                    error = true;
                } else {
                    // XXX: is_member_of_source_char_set(c_)
                    transit(ScannerState::kState272_274_271, c_);
                }
            }
            break;
        }
        case ScannerState::kState136: {
            mark();
            finish(TokenType::kPunctuator); // "&="
            break;
        }
        case ScannerState::kState112: {
            mark();
            finish(TokenType::kPunctuator); // "%="
            break;
        }
        case ScannerState::kState26: {
            mark();
            finish(TokenType::kPunctuator); // "--"
            break;
        }
        case ScannerState::kState146: {
            mark();
            finish(TokenType::kComma); // ","
            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D..."
            }
            break;
        }
        case ScannerState::kState72_138_139: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState140, c_);
            } else {
                finish(TokenType::kPunctuator); // "^"
            }
            break;
        }
        case ScannerState::kState128_129_48_49_54_60_61: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState62, c_);
            } else if (c_ == to_c32('>')) {
                transit(ScannerState::kState130_131_50, c_);
            } else {
                finish(TokenType::kPunctuator); // ">"
            }
            break;
        }
        case ScannerState::kState182_184_94_14_95: {
            mark();
            if (c_ == to_c32('.')) {
                transit(ScannerState::kState96_97, c_);
            } else if (is_digit(c_)) {   // 'D'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188, c_);
            } else {
                finish(TokenType::kPunctuator); // "."
            }
            break;
        }
        case ScannerState::kState96_97: {
            if (c_ == to_c32('.')) {
                transit(ScannerState::kState98, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "DCCCC"
            }
            break;
        }
        case ScannerState::kState70: {
            mark();
            finish(TokenType::kPunctuator); // "!="
            break;
        }
        case ScannerState::kState148_150_151: {
            mark();
            if (c_ == to_c32('#')) {
                transit(ScannerState::kState152, c_);
            } else {
                finish(TokenType::kHash); // "#"
            }
            break;
        }
        case ScannerState::kState275: {
            mark();
            finish(TokenType::kStringLiteral);  // """"""
            break;
        }
        case ScannerState::kState232_225_244_264_249_234_269: {
            mark();
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == to_c32('\'')) {
                transit(ScannerState::kState250_251, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "L"
            }
            break;
        }
        case ScannerState::kState12: {
            mark();
            finish(TokenType::kRightBrace); // "}"
            break;
        }
        case ScannerState::kState242_262_232_249_234_269_223: {
            mark();
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == to_c32('\'')) {
                transit(ScannerState::kState250_251, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "U"
            }
            break;
        }
        case ScannerState::kState4: {
            mark();
            finish(TokenType::kRightBracket); // "]"
            break;
        }
        case ScannerState::kState144: {
            mark();
            finish(TokenType::kPunctuator); // "|="
            break;
        }
        case ScannerState::kState192_195_196_198_200_202_204_205_206_209_211_212_213_214_186_188: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D\'N"
            }
            break;
        }
        case ScannerState::kState140: {
            mark();
            finish(TokenType::kPunctuator); // "^="
            break;
        }
        case ScannerState::kState232_234_227: {
            mark();
            if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {  // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                //error = contains_invalid_ucn_for_identifier(cseq_);
                finish(TokenType::kIdentifier); // "S"
            }
            break;
        }
        case ScannerState::kState250_251: {
            if (c_ == to_c32('\\')) {   // 'c'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState251_252_253);
            } else {    // 'c'
                if (is_nl(c_) || eof()) {
                    error = true;
                } else {
                    // XXX: is_member_of_source_char_set(c_)
                    transit(ScannerState::kState251_252_253, c_);
                }
            }
            break;
        }
        case ScannerState::kState32_114_115_20_21: {
            mark();
            if (c_ == to_c32('+')) {
                transit(ScannerState::kState22, c_);
            } else if (c_ == to_c32('=')) {
                transit(ScannerState::kState116, c_);
            } else {
                finish(TokenType::kPunctuator); // "+"
            }
            break;
        }
        case ScannerState::kState40_106_107: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState108, c_);
            } else if (c_ == to_c32('/')) {
                transit(ScannerState::kLineComment, c_);
            } else if (c_ == to_c32('*')) {
                transit(ScannerState::kBlockComment, c_);
            } else {
                finish(TokenType::kPunctuator); // "/"
            }
            break;
        }
        case ScannerState::kState2: {
            mark();
            finish(TokenType::kLeftBracket); // "["
            break;
        }
        case ScannerState::kState104: {
            mark();
            finish(TokenType::kPunctuator); // "*="
            break;
        }
        case ScannerState::kState84: {
            mark();
            finish(TokenType::kPunctuator); // "?"
            break;
        }
        case ScannerState::kState162_163_122_123_44_45_52_56_57_154_155: {
            mark();
            if (c_ == to_c32('%')) {
                transit(ScannerState::kState164, c_);
            } else if (c_ == to_c32(':')) {
                transit(ScannerState::kState156, c_);
            } else if (c_ == to_c32('<')) {
                transit(ScannerState::kState124_125_46, c_);
            } else if (c_ == to_c32('=')) {
                transit(ScannerState::kState58, c_);
            } else {
                finish(TokenType::kPunctuator); // "<"
            }
            break;
        }
        case ScannerState::kState164: {
            mark();
            finish(TokenType::kLeftBrace);    // "<%"
            break;
        }
        case ScannerState::kState98: {
            mark();
            finish(TokenType::kEllipsis); // "..."
            break;
        }
        case ScannerState::kState152: {
            mark();
            finish(TokenType::kHashHash); // "##"
            break;
        }
        case ScannerState::kState6: {
            mark();
            finish(TokenType::kLeftParenthesis); // "("
            break;
        }
        case ScannerState::kState168: {
            mark();
            finish(TokenType::kRightBrace);    // "%>"
            break;
        }
        case ScannerState::kState176_177_172: {
            mark();
            if (c_ == to_c32('%')) {
                transit(ScannerState::kState178_179, c_);
            } else {
                finish(TokenType::kHash);    // "%:"
            }
            break;
        }
        case ScannerState::kState203_207: {
            if (c_ == to_c32('+') || c_ == to_c32('-')) {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState62: {
            mark();
            finish(TokenType::kPunctuator); // ">="
            break;
        }
        case ScannerState::kState156: {
            mark();
            finish(TokenType::kLeftBracket);    // "<:"
            break;
        }
        case ScannerState::kState232_233: {
            mark();
            if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {  // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                //error = contains_invalid_ucn_for_identifier(cseq_);
                finish(TokenType::kIdentifier); // "uCCCCCC"
            }
            break;
        }
        case ScannerState::kState66: {
            mark();
            finish(TokenType::kPunctuator); // "=="
            break;
        }
        case ScannerState::kState124_125_46: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState126, c_);
            } else {
                finish(TokenType::kPunctuator); // "<<"
            }
            break;
        }
        case ScannerState::kState116: {
            mark();
            finish(TokenType::kPunctuator); // "+="
            break;
        }
        case ScannerState::kState120: {
            mark();
            finish(TokenType::kPunctuator); // "-="
            break;
        }
        case ScannerState::kState78: {
            mark();
            finish(TokenType::kPunctuator); // "&&"
            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188: {
            mark();
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == to_c32('.')) {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == to_c32('E')) {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == to_c32('P')) {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == to_c32('e')) {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == to_c32('p')) {
                transit(ScannerState::kState201_207, c_);
            } else if (is_digit(c_) || is_nondigit(c_) || is_xid_continue(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == to_c32('\\')) {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "DEgggg"
            }
            break;
        }
        case ScannerState::kState18: {
            mark();
            finish(TokenType::kPunctuator); // "->"
            break;
        }
        case ScannerState::kState108: {
            mark();
            finish(TokenType::kPunctuator); // "/="
            break;
        }
        case ScannerState::kState130_131_50: {
            mark();
            if (c_ == to_c32('=')) {
                transit(ScannerState::kState132, c_);
            } else {
                finish(TokenType::kPunctuator); // ">>"
            }
            break;
        }
        case ScannerState::kState272_274_271: {
            if (c_ == to_c32('"')) {
                transit(ScannerState::kState275, c_);
            } else if (c_ == to_c32('\\')) {    // 's'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState272_274_271);
            } else {    // 's'
                if (is_nl(c_) || eof()) {
                    error = true;
                } else {
                    // XXX: is_member_of_source_char_set(c_)
                    transit(ScannerState::kState272_274_271, c_);
                }
            }
            break;
        }
        case ScannerState::kState251_252_253: {
            if (c_ == to_c32('\'')) {
                transit(ScannerState::kState254, c_);
            } else if (c_ == to_c32('\\')) {    // 'c'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState251_252_253);
            } else {    // 'c'
                if (is_nl(c_) || eof()) {
                    error = true;
                } else {
                    // XXX: is_member_of_source_char_set(c_)
                    transit(ScannerState::kState251_252_253, c_);
                }
            }
            break;
        }
        case ScannerState::kState58: {
            mark();
            finish(TokenType::kPunctuator); // "<="
            break;
        }
        case ScannerState::kState22: {
            mark();
            finish(TokenType::kPunctuator); // "++"
            break;
        }
        case ScannerState::kState132: {
            mark();
            finish(TokenType::kPunctuator); // ">>="
            break;
        }
        case ScannerState::kState254: {
            mark();
            finish(TokenType::kCharacterConstant);  // "\'\'\'\'\'c\'"
            break;
        }
        case ScannerState::kState126: {
            mark();
            finish(TokenType::kPunctuator); // "<<="
            break;
        }
        case ScannerState::kState178_179: {
            if (c_ == to_c32(':')) {
                transit(ScannerState::kState180, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState180: {
            mark();
            finish(TokenType::kHashHash);   // "%:%:"
            break;
        }

        case ScannerState::kLineComment: {
            while (!(is_nl(c_) || eof())) {
                consume(c_);
            }

            finish(TokenType::kComment);
            break;
        }
        case ScannerState::kBlockComment: {
            Char32 prev = to_c32('\0');
            while (true) {
                if (eof()) {
                    // error(Token(Token::kNullTokenValue, line, column), /* コメントが終了しなかった。 */);
                    break;
                }
                if (c_ == to_c32('/') && prev == to_c32('*')) {
                    consume(c_);
                    break;
                }
                prev = c_;
                consume(c_);
            }

            finish(TokenType::kComment);
            break;
        }
        case ScannerState::kWhiteSpaces: {
            while (is_ws(c_)) {
                consume(c_);
            }

            finish(TokenType::kWhiteSpace);
            break;
        }
        case ScannerState::kLineBreak: {
            if (c_ == to_c32('\n')) {
                consume(c_);
            }

            finish(TokenType::kNewLine);
            break;
        }
        case ScannerState::kStateUniversalCharacterNameInitial: {
            if (cseq_.length() >= numeric_limits<decltype(ucn_digit_start_)>::max() - 1) {
                throw runtime_error(__func__);
            }
            ucn_digit_start_ = static_cast<decltype(ucn_digit_start_)>(cseq_.length() + 1);

            if (c_ == to_c32('U')) {
                transit(ScannerState::kStateUniversalCharacterName16_17, c_);
            } else if (c_ == to_c32('u')) {
                transit(ScannerState::kStateUniversalCharacterName4_5, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName16_17: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName18_19, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName4_5: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName6_7, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName18_19: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName20_21, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName20_21: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName22_23, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName6_7: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName8_9, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName22_23: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName24_25, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName24_25: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName26_27, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName26_27: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName28_29, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName8_9: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName10_11, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName10_11: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName12, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName28_29: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName30_31, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName30_31: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateUniversalCharacterName32, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateUniversalCharacterName12: {
            mark();

            uint32_t n = parse_ucn_digits(&cseq_[ucn_digit_start_], & cseq_[cseq_.length()]);
            switch (return_state_) {
            case ScannerState::kState232_234_227:
                if (!is_xid_start(to_c32(n))) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidIdentifierStartError);
                }
                break;

            case ScannerState::kState232_233:
            case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188:
            case ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188:
                if (!is_xid_continue(to_c32(n))) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidIdentifierContinueError);
                }
                break;

            default:
                diag_.fatal_error(sources_.current_source_pointer(), { line_number_, buf_i_ }, as_internal(__func__));
                break;
            }

            finish();   // \uXXXX
            break;
        }
        case ScannerState::kStateUniversalCharacterName32: {
            mark();

            uint32_t n = parse_ucn_digits(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()]);
            switch (return_state_) {
            case ScannerState::kState232_234_227:
                if (!is_xid_start(to_c32(n))) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidIdentifierStartError);
                }
                break;

            case ScannerState::kState232_233:
            case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188:
            case ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188:
                if (!is_xid_continue(to_c32(n))) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidIdentifierContinueError);
                }
                break;

            default:
                diag_.fatal_error(sources_.current_source_pointer(), { line_number_, buf_i_ }, as_internal(__func__));
                break;
            }

            finish();   // \UXXXXXXXX
            break;
        }
        case ScannerState::kStateEscapeSequenceInitial: {
            switch (to_int(c_)) {
            case '"':   // 6
            case '\'':  // 4
            case '?':   // 8
            case '\\':  // 10
            case 'a':   // 12
            case 'b':   // 14
            case 'f':   // 16
            case 'n':   // 18
            case 'r':   // 20
            case 't':   // 22
            case 'v':   // 24
                finish(c_);
                break;

            case 'u':
                if (cseq_.length() >= numeric_limits<decltype(ucn_digit_start_)>::max() - 1) {
                    throw runtime_error(__func__);
                }
                ucn_digit_start_ = static_cast<decltype(ucn_digit_start_)>(cseq_.length() + 1);
                transit(ScannerState::kStateEscapeSequence42_43, c_);
                break;

            case 'U':
                if (cseq_.length() >= numeric_limits<decltype(ucn_digit_start_)>::max() - 1) {
                    throw runtime_error(__func__);
                }
                ucn_digit_start_ = static_cast<decltype(ucn_digit_start_)>(cseq_.length() + 1);
                transit(ScannerState::kStateEscapeSequence52_53, c_);
                break;

            case 'x':
                transit(ScannerState::kStateEscapeSequence38_39, c_);
                break;

            default:
                if (is_oct(c_)) {
                    transit(ScannerState::kStateEscapeSequence32_33_26_28_29, c_);
                } else {
                    diag_.warning(sources_.current_source_pointer(), { line_number_, buf_i_ }, kUnknownEscapeSequenceWarning);
                    finish();
                }
                break;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence52_53: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence54_55, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence54_55: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence56_57, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence56_57: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence58_59, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence58_59: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence60_61, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence38_39: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence40_39, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidHexadecimalEscapeSequenceFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence42_43: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence44_45, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence40_39: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence40_39, c_);
            } else {
                // \xX...
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence44_45: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence46_47, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence46_47: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence48_49, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence32_33_26_28_29: {
            if (is_oct(c_)) {
                transit(ScannerState::kStateEscapeSequence34_35_30, c_);
            } else {
                // \O
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence60_61: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence62_63, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence34_35_30: {
            if (is_oct(c_)) {
                // \OOO
                finish(c_); // ScannerState::kStateEscapeSequence36
            } else {
                // \OO
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence48_49: {
            if (is_hex(c_)) {
                // \uXXXX
                finish(c_); // ScannerState::kStateEscapeSequence50

                uint32_t n = parse_ucn_digits(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()]);
                if (!is_valid_ucn(n)) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameCodePointError);
                }
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence62_63: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence64_65, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence64_65: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence66_67, c_);
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence66_67: {
            if (is_hex(c_)) {
                // \UXXXXXXXX
                finish(c_); // ScannerState::kStateEscapeSequence68);

                uint32_t n = parse_ucn_digits(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()]);
                if (!is_valid_ucn(n)) {
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameCodePointError);
                }
            } else {
                diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kInvalidUniversalCharacterNameFormatError);
                finish();
            }
            break;
        }
        case ScannerState::kStateHeaderNameInitial: {
            Char32 close_char = (cseq_[0] == '<') ? to_c32('>') : to_c32('"');
            if (c_ == close_char) {
                transit(ScannerState::kStateHeaderNameA1, c_);
            } else {
                if (is_nl(c_) || eof()) {
                    // 行末まで読み取ったので、ヘッダー名としては不正で、このままエラーで返す。
                    diag_.error(sources_.current_source_pointer(), { line_number_, buf_i_ }, kUnclosedHeaderNameError);
                    error = true;
                } else {
                    transit(ScannerState::kStateHeaderNameInitial, c_);
                }
            }
            break;
        }
        case ScannerState::kStateHeaderNameA1: {
            finish(TokenType::kHeaderName);
            break;
        }
        default: {
            // fatal_error();
            error = true;
        }
        }   // switch
    }

    if (error) {
        type_ = TokenType::kNonWhiteSpaceCharacter;
    }

    return Token(cseq_, type_, line_number, column);
}

bool Scanner::is_support_trigraph() {
    return trigraph_;
}

void Scanner::state_hint(ScannerHint hint) {
    hint_ = hint;
}

std::uint32_t Scanner::line_number() {
    return line_number_;
}

void Scanner::line_number(std::uint32_t value) {
    line_number_ = value;
}

std::uint32_t Scanner::column() {
    return buf_i_;
}

bool Scanner::eof() const {
    return eof_;
}

Scanner::Char32 Scanner::get() {
    if (buf_i_ >= buf_.length()) {
        if (readline() != 0) {
            eof_ = true;
            return to_c32(0);
        }
    }

    if (buf_i_ >= buf_.length()) {
        eof_ = true;
        return to_c32(0);
    }

    Char32 c;
    auto size = c8_to_c32(&buf_[buf_i_], buf_.length() - buf_i_, c);
    if (size == 0) {
        return to_c32(0);
    }
    buf_i_ += size;

    return c;
}

int Scanner::getline(std::string& result) {
    int c = input_.get();
    if (c == EOF) {
        return -1;
    }

    result.clear();
    do {
        result += to_c(c);

        if (c == '\r') {
            int c2 = input_.get();

            if (c2 == EOF) {
                if (input_.eof()) {
                    // 一旦、ここまでに読み取れた分を呼び出し元に返す。
                    break;
                } else {
                    // でなければエラー。
                    return -1;
                }
            } else if (c2 == '\n') {
                result += to_c(c2);
            } else {
                input_.putback(to_c(c2));
            }
            break;
        }
        if (c == '\n') {
            break;
        }

        c = input_.get();
        if (c == EOF && !input_.eof()) {
            return -1;
        }
    } while (c != EOF);

    return 0;
}

int Scanner::readline() {
    if (input_.eof()) {
        return EOF;
    }

    bool more_splicing = false;
    string s;
    buf_.clear();
    do {
        if (buf_.length() >= numeric_limits<decltype(buf_i_)>::max() ||
            line_number_ >= numeric_limits<decltype(line_number_)>::max()) {
            //  XXX
            return EOF;
        }

        if (getline(s) != 0) {
            if (more_splicing) {
                // TODO: 次の行が全く無かった警告。
                break;
            } else {
                return EOF;
            }
        }

        //  1.
        if (is_support_trigraph()) {
            replace_trigraphs(s);
        }

        //  2.
        more_splicing = splice_source_line(buf_, s);

        line_number_++;
    } while (more_splicing);

    buf_ = normalize_string(buf_);
    buf_i_ = 0;

    return 0;
}

std::string Scanner::replace_trigraphs(std::string& s) {
    constexpr int kTrigraphComponents = 3;
    string::size_type r = 0;
    string::size_type w = 0;

    if (s.length() < kTrigraphComponents) {
        return s;
    }

    while (r < s.length() - kTrigraphComponents) {
        if ((s[r] != '?') || (s[r + 1] != '?')) {
            s[w] = s[r];
            r++;
            w++;
        } else {
            char t2 = s[r + 2];
            char c;
            switch (t2) {
            case '=':
                c = '#';
                break;
            case ')':
                c = ']';
                break;
            case '!':
                c = '|';
                break;
            case '(':
                c = '[';
                break;
            case '\'':
                c = '^';
                break;
            case '>':
                c = '}';
                break;
            case '/':
                c = '\\';
                break;
            case '<':
                c = '{';
                break;
            case '-':
                c = '~';
                break;
            default:
                c = t2;
                break;
            }
            if (c != t2) {
                s[w] = c;
                r += kTrigraphComponents;
                w += 1;
            } else {
                s[w] = s[r];
                r++;
                w++;
            }
        }
    }
    while (r < s.length()) {
        s[w] = s[r];
        r++;
        w++;
    }

    s.erase(w);

    return s;
}

bool Scanner::splice_source_line(std::string& logical_line, std::string& physical_line) {
    bool more_splicing = false;

    auto not_nl = find_if(physical_line.rbegin(), physical_line.rend(), [](const auto& c) { return !is_nl(to_c32(c)); });
    if (not_nl == physical_line.rend()) {
        // TODO: 改行で終わらないのでエラー。尚、空のファイルは初回の getlineで弾かれる。
    }
    if (not_nl != physical_line.rend()) {
        auto last_c = find_if(not_nl, physical_line.rend(), [](const auto& c) { return !is_ws(to_c32(c)); });
        if (not_nl == last_c && *last_c == '\\') {
            auto back_slash = prev(last_c.base());
            physical_line.erase(back_slash, physical_line.end());
            more_splicing = true;
        }
        if (not_nl != last_c && last_c != physical_line.rend() && *last_c != '\\') {
            // TODO: バックスラッシュと改行までの間に空白が有るという警告かエラー。連結しない。
        }
    }

    logical_line += physical_line;

    return more_splicing;
}

void Scanner::consume(Char32 c) {
    char u8[4] = {};
    int len = c32_to_c8(c, u8);
    cseq_.append(u8, len);
    c_ = get();
}

void Scanner::transit(ScannerState next_state, Char32 c) {
    consume(c);
    state_ = next_state;
}

void Scanner::transit(ScannerState next_state, Char32 c, ScannerState return_state) {
    if (return_state_ != ScannerState::kInitial) {
        throw runtime_error(__func__);
    }

    consume(c);
    state_ = next_state;
    return_state_ = return_state;
}

void Scanner::finish(TokenType token_type) {
    type_ = token_type;
    match_ = true;
}

void Scanner::finish() {
    if (return_state_ == ScannerState::kInitial) {
        throw runtime_error(__func__);
    }

    state_ = return_state_;
    return_state_ = ScannerState::kInitial;
}

void Scanner::finish(Char32 c) {
    if (return_state_ == ScannerState::kInitial) {
        throw runtime_error(__func__);
    }

    consume(c);
    state_ = return_state_;
    return_state_ = ScannerState::kInitial;
}

void Scanner::mark() {
    buf_i_mark_ = buf_i_;
}

void Scanner::reset(Char32String& cseq) {
    if (buf_i_mark_ == 0) {
        throw std::runtime_error(__func__);
    }
    if (buf_i_ <= buf_i_mark_) {
        throw std::runtime_error(__func__);
    }

    uint32_t n = (buf_i_ - buf_i_mark_) - 1;
    cseq.erase(cseq.length() - n);
    buf_i_ = buf_i_mark_;
    c_ = get();
}

void Scanner::clear_mark() {
    buf_i_mark_ = 0;
}

}   //  namespace pp
