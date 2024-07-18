#include "preprocessor/calculator.h"

#include <bit>
#include <cassert>
#include <vector>
#include "util/utility.h"

using namespace lib::util;
using namespace std;

namespace {

using namespace pp;

struct IntegerSuffix {
    string_view suffix;
    IntegerType type;
};

struct IntegerSuffixLess {
    constexpr bool operator()(const IntegerSuffix& lhs, const IntegerSuffix& rhs) const {
        return lhs.suffix < rhs.suffix;
    }

    constexpr bool operator()(const IntegerSuffix& lhs, const std::string_view& rhs) const {
        return lhs.suffix < rhs;
    }

    constexpr bool operator()(const std::string_view& lhs, const IntegerSuffix& rhs) const {
        return lhs < rhs.suffix;
    }
};

constexpr IntegerSuffix integer_suffixes_sorted[] = {
    { "L", IntegerType::kLong },
    { "LL", IntegerType::kLongLong },
    { "LLU", IntegerType::kULongLong },
    { "LLu", IntegerType::kULongLong },
    { "LU", IntegerType::kULong },
    { "Lu", IntegerType::kULong },
    { "U", IntegerType::kUInt },
    { "UL", IntegerType::kULong },
    { "ULL", IntegerType::kULongLong },
    { "UWB", IntegerType::kUBitInt },
    { "Ul", IntegerType::kULong },
    { "Ull", IntegerType::kULongLong },
    { "Uwb", IntegerType::kUBitInt },
    { "WB", IntegerType::kBitInt },
    { "WBU", IntegerType::kUBitInt },
    { "WBu", IntegerType::kUBitInt },
    { "l", IntegerType::kLong },
    { "lU", IntegerType::kULong },
    { "ll", IntegerType::kLongLong },
    { "llU", IntegerType::kULongLong },
    { "llu", IntegerType::kULongLong },
    { "lu", IntegerType::kULong },
    { "u", IntegerType::kUInt },
    { "uL", IntegerType::kULong },
    { "uLL", IntegerType::kULongLong },
    { "uWB", IntegerType::kUBitInt },
    { "ul", IntegerType::kULong },
    { "ull", IntegerType::kULongLong },
    { "uwb", IntegerType::kUBitInt },
    { "wb", IntegerType::kBitInt },
    { "wbU", IntegerType::kUBitInt },
    { "wbu", IntegerType::kUBitInt },
};

struct IntegerMax {
    IntegerType type;
    target_uintmax_t max;
};

// 接尾辞 Uの付いていない 10進数の値
constexpr IntegerMax type_list_signed_decimal[] = {
    { IntegerType::kInt, CC_TARGET_INT_MAX },
    { IntegerType::kLong, CC_TARGET_LONG_MAX },
    { IntegerType::kLongLong, CC_TARGET_LLONG_MAX },
};

// 接尾辞 Uの付いていない 10進数以外の値
constexpr IntegerMax type_list_signed_others[] = {
    { IntegerType::kInt, CC_TARGET_INT_MAX },
    { IntegerType::kUInt, CC_TARGET_UINT_MAX },
    { IntegerType::kLong, CC_TARGET_LONG_MAX },
    { IntegerType::kULong, CC_TARGET_ULONG_MAX },
    { IntegerType::kLongLong, CC_TARGET_LLONG_MAX },
    { IntegerType::kULongLong, CC_TARGET_ULLONG_MAX },
};

// 接尾辞 Uの付いている値
constexpr IntegerMax type_list_unsigned[] = {
    { IntegerType::kUInt, CC_TARGET_UINT_MAX },
    { IntegerType::kULong, CC_TARGET_ULONG_MAX },
    { IntegerType::kULongLong, CC_TARGET_ULLONG_MAX },
};

}   //  anonymous namespace

namespace pp {

void init_calculator() {
}

std::errc parse_int(const std::string& s, Integer& result) {
    if (s.empty()) {
        return errc::invalid_argument;
    }

    string::size_type i = 0;
    int base;
    if (s[i] == '0') {
        switch (tolower(s[i + 1])) {
        case 'x': {
            base = 16;
            i += 2; // skip "0x"
            break;
        }
        case 'b': {
            base = 2;
            i += 2; // skip "0b"
            break;
        }
        default: {
            base = 8;
            // 単体の 0かもしれないので先頭のゼロはスキップしない。
            break;
        }
        }   // switch
    } else {
        if (isdigit(s[i])) {
            base = 10;
        } else {
            base = 0;
        }
    }
    if (base == 0) {
        return errc::invalid_argument;
    }
    if (i >= s.length()) {
        return errc::invalid_argument;
    }

    constexpr static const char digits16[] = "0123456789abcdef";
    target_uintmax_t n = 0;
    bool separator = false;
    while (i < s.length()) {
        auto it = binary_find(digits16, digits16 + base, tolower(s[i]));
        if (it != (digits16 + base)) {
            if (n > (numeric_limits<decltype(n)>::max() / base)) {
                return errc::result_out_of_range;
            }
            n *= base;

            auto d = static_cast<decltype(n)>(distance(digits16, it));
            if (n > (numeric_limits<decltype(n)>::max() - d)) {
                return errc::result_out_of_range;
            }
            n += d;

            separator = false;
        } else {
            if (s[i] == '\'') {
                if (separator) {
                    // 字句的エラー
                    return errc::invalid_argument;
                }
                separator = true;
                // skip
            } else {
                // 不正な文字かもしれないし、接尾辞かもしれない。後続の処理に任せるので、returnではなく breakする。
                break;
            }
        }

        ++i;
    }

    // 指定されている型を得る。接尾辞が有ればその通りに、また、接尾辞が無ければ signed int。
    IntegerType suffix;
    if (i == s.length()) {
        suffix = IntegerType::kInt;
    } else {
        string_view maybe_suffix(&s[i], s.length() - i);
        auto found = binary_find(begin(integer_suffixes_sorted), end(integer_suffixes_sorted), maybe_suffix, IntegerSuffixLess{});
        if ((found == end(integer_suffixes_sorted)) || (i + size(found->suffix) != s.length())) {
            // 接尾辞が不明なのでエラー
            return errc::invalid_argument;
        }

        suffix = found->type;
    }

    // _BitIntの場合は、指定されている数値を表現できるビット数を求める。
    // それ以外の場合は、指定されている数値を表現できる実際の型を求める。
    IntegerType result_type;
    if (suffix.base() == IntegerType::kBitIntBase) {
        auto width = (n == 0) ? 1 : bit_width(n);   // 1: どんな数値でも最低 1ビットは必要なので。
        if (suffix.is_signed()) {
            if (width >= CC_TARGET_BITINT_MAXWIDTH) {
                return errc::result_out_of_range;
            }
            width += 1; // 1: 符号ビット

            assert(IntegerType::is_valid_bitint_width(width));
            result_type = IntegerType::bitint(width);
        } else {
            assert(IntegerType::is_valid_ubitint_width(width));
            result_type = IntegerType::ubitint(width);
        }
    } else {
        const IntegerMax* first;
        const IntegerMax* last;
        if (suffix.is_signed()) {
            if (base == 10) {
                first = begin(type_list_signed_decimal);
                last = end(type_list_signed_decimal);
            } else {
                first = begin(type_list_signed_others);
                last = end(type_list_signed_others);
            }
        } else {
            first = begin(type_list_unsigned);
            last = end(type_list_unsigned);
        }

        // 指定されている型を開始点とする。
        const auto specified = find_if(first, last, [&suffix](const auto& x) { return suffix == x.type; });

        // 数値が実際に収まるような、指定されている型以上の幅を持つ型を探す。
        const auto actual = find_if(specified, last, [&n](const auto& x) { return n <= x.max; });
        if (actual != last) {
            // Lが指定されているのに LLの範囲の値だったりする事が有り得るが、とりあえず見つかった
            // 表現可能な型を素直にそのまま使う。特に何もしない。infoレベルで診断メッセージを出したり
            // できると良いかも知れない。
            result_type = actual->type;
        } else {
            if (suffix.is_signed() && base == 10) {
                // signedでしかチェックされない場合でここに来た時はエラーとして返す。
                return errc::result_out_of_range;

                // unsignedとして返す場合はこんな感じだろうか。
                //const auto usuffix = IntegerType::make_unsigned(suffix);
                //const auto ufirst = begin(type_list_unsigned);
                //const auto ulast = end(type_list_unsigned);
                //const auto umatch = find_if(ufirst, ulast, [&usuffix](const auto& x) { return usuffix == x.type; });
                //assert(umatch != ulast);

                //const auto temp_umatch = find_if(umatch, ulast, [&n = as_const(n)](const auto& x) { return n <= x.max; });
                //assert(temp_umatch != ulast);

                //// warning(/* 数値が signedの範囲には収まらないが unsignedの範囲には収まるので、unsignedにした。 */);
                //result_type = temp_umatch->type;
            } else {
                // テーブル類の作り間違い、target_uintmax_tより大きな幅を持つ独自型への対応を忘れたなど
                assert(false);
                result_type = IntegerType::kNoType;
            }
        }
    }

    result.type = result_type;
    result.value = n;
    return errc{};
}

std::string IntegerType::to_string() const {
    switch (value()) {
    case IntegerType::kInt:
        return "kInt";
    case IntegerType::kLong:
        return "kLong";
    case IntegerType::kLongLong:
        return "kLongLong";
    case IntegerType::kUInt:
        return "kUInt";
    case IntegerType::kULong:
        return "kULong";
    case IntegerType::kULongLong:
        return "kULongLong";
    default:
        if (base() == IntegerType::kBitIntBase) {
            if (is_signed()) {
                return format("kBitInt({})", width());
            } else {
                return format("kUBitInt({})", width());
            }
        } else {
            return "(IntegerType?)";
        }
        break;
    }
}

}   //  namespace pp
