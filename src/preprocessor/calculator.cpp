#include <preprocessor/calculator.h>

#include <cassert>
#include <vector>
#include <preprocessor/utility.h>

using namespace std;

namespace {

using namespace pp;

std::vector<std::string_view> integer_suffixes = {
    "L",
    "LU",
    "Lu",
    "l",
    "lU",
    "lu",
    "LL",
    "LLU",
    "LLu",
    "ll",
    "llU",
    "llu",
    "U",
    "UL",
    "ULL",
    "Ul",
    "Ull",
    "u",
    "uL",
    "uLL",
    "ul",
    "ull",
};

}	//  anonymous namespace

namespace pp {

void init_calculator() {
    sort(integer_suffixes.begin(), integer_suffixes.end());
}

bool parse_int(const std::string& s, target_intmax_t* result) {
    assert(result != nullptr);

    *result = 0;
    if (s.empty()) {
        return false;
    }

    size_t i = 0;
    if (s[i] == '+' || s[i] == '-') {
        i++;
    }
    int base = (s[i] != '0')
        ? 10
        : ((s.length() >= (i + 2)) && (s[i + 1] == 'x') ? 16 : 8);

    try {
        size_t next = 0;
        target_intmax_t n = stoul(s, &next, base);
        if (next != s.length()) {
            string_view maybe_suffix(&s[next]);
            auto found = binary_find(begin(integer_suffixes), end(integer_suffixes), maybe_suffix);
            if ((found == end(integer_suffixes)) || (next + size(*found) != s.length())) {
                return false;
            }
        }

        *result = n;
        return true;
    }
    catch (...) {
        return false;
    }
}

}   //  namespace pp
