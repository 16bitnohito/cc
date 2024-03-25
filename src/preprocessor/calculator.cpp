#include "preprocessor/calculator.h"

#include <cassert>
#include <vector>
#include "util/utility.h"

using namespace lib::util;
using namespace std;

namespace {

using namespace pp;

constexpr std::string_view integer_suffixes_sorted[] = {
    "L",
    "LL",
    "LLU",
    "LLu",
    "LU",
    "Lu",
    "U",
    "UL",
    "ULL",
    "UWB",
    "Ul",
    "Ull",
    "Uwb",
    "WB",
    "WBU",
    "WBu",
    "l",
    "lU",
    "ll",
    "llU",
    "llu",
    "lu",
    "u",
    "uL",
    "uLL",
    "uWB",
    "ul",
    "ull",
    "uwb",
    "wb",
    "wbU",
    "wbu",
};

}   //  anonymous namespace

namespace pp {

void init_calculator() {
}

bool parse_int(const std::string& s, target_intmax_t* result) {
    assert(result != nullptr);

    if (s.empty()) {
        return false;
    }

    string::size_type i = 0;
    int base = 0;
    if (s[i] != '0') {
        if (isdigit(s[i])) {
            base = 10;
        }
    } else {
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
    }
    if (base == 0) {
        return false;
    }
    if (i >= s.length()) {
        return false;
    }

    constexpr char digits16[] = "0123456789abcdef";
    target_uintmax_t n = 0;
    bool separator = false;
    while (i < s.length()) {
        auto it = binary_find(digits16, digits16 + base, tolower(s[i]));
        if (it != (digits16 + base)) {
            n *= base;
            n += static_cast<decltype(n)>(distance(digits16, it));
            separator = false;
        } else {
            if (s[i] == '\'') {
                if (separator) {
                    // 字句的エラー
                    return false;
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

    if (i < s.length()) {
        string_view maybe_suffix(&s[i], s.length() - i);
        auto found = binary_find(begin(integer_suffixes_sorted), end(integer_suffixes_sorted), maybe_suffix);
        if ((found == end(integer_suffixes_sorted)) || (i + size(*found) != s.length())) {
            return false;
        }
    }

    *result = n;
    return true;
}

}   //  namespace pp
