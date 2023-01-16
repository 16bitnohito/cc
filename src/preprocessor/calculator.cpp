#include <preprocessor/calculator.h>

#include <cassert>
#include <vector>
#include <preprocessor/utility.h>

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
    "Ul",
    "Ull",
    "l",
    "lU",
    "ll",
    "llU",
    "llu",
    "lu",
    "u",
    "uL",
    "uLL",
    "ul",
    "ull",
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

    // トークンに符号が含まれることはないが、書いてしまったので残しておく。
    string::size_type i = 0;
    int sign = 1;
    if (s[i] == '+') {
        ++i;
    } else if (s[i] == '-') {
        sign = -1;
        ++i;
    }

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
            break;
        }
        }
    }
    if (base == 0) {
        return false;
    }
    if (i >= s.length()) {
        return false;
    }

    constexpr char digits16[] = "0123456789abcdef";
    target_intmax_t n = 0;
    while (i < s.length()) {
        auto it = binary_find(digits16, digits16 + base, tolower(s[i]));
        if (it != (digits16 + base)) {
            n *= base;
            n += static_cast<target_intmax_t>(distance(digits16, it));
        } else {
            if (s[i] == '\'') {
                // skip
            } else {
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

    *result = sign * n;
    return true;
}

}   //  namespace pp
