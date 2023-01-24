#include <preprocessor/scanner.h>

#include <algorithm>
#include <cassert>
#include <preprocessor/diagnostics.h>
#include <preprocessor/utility.h>

using namespace std;

namespace {

enum {
    kWsChar = 256,
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

int char_kind(int c) {
    if (c == EOF) {
        return EOF;
    } else {
        auto k = kInitial[c];
        switch (k) {
        case kWsChar:
        case kDigit:
        case kLetter:
            return k;
        default:
            return c;
        }
    }
}

inline
bool is_hex(int c) {
    return isxdigit(c) != 0;
}

inline
bool is_dec(int c) {
    return isdigit(c) != 0;
}

inline
bool is_oct(int c) {
    return ('0' <= c && c <= '7');
}

inline
bool is_esc_seq(int c) {
    return c == '\'' || c == '"' || c == '?' || c == '\\' ||
            c == 'a' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v';
}

inline
bool is_nondigit(int c) {
    return (c == '_' || isalpha(c)) || (c == '\\');
}

inline
bool is_ws(int c) {
    return c == '\x09' || c == '\x0b' || c == '\x0c' || c == '\x20';
}

inline
bool is_nl(int c) {
    return c == '\x0a' || c == '\x0d';
}

inline
bool is_eol(int c) {
    return is_nl(c) || c == EOF;
}

//  D.1 Ranges of characters allowed
constexpr uint32_t ucn_allowed[][3] = {
    { 0x00A8, 0x00A8, 1 },
    { 0x00AA, 0x00AA, 1 },
    { 0x00AD, 0x00AD, 1 },
    { 0x00AF, 0x00AF, 1 },
    { 0x00B2, 0x00B5, 1 },
    { 0x00B7, 0x00BA, 1 },
    { 0x00BC, 0x00BE, 1 },
    { 0x00C0, 0x00D6, 1 },
    { 0x00D8, 0x00F6, 1 },
    { 0x00F8, 0x00FF, 1 },

    { 0x0100, 0x167F, 2 },
    { 0x1681, 0x180D, 2 },
    { 0x180F, 0x1FFF, 2 },

    { 0x200B, 0x200D, 3 },
    { 0x202A, 0x202E, 3 },
    { 0x203F, 0x2040, 3 },
    { 0x2054, 0x2054, 3 },
    { 0x2060, 0x206F, 3 },

    { 0x2070, 0x218F, 4 },
    { 0x2460, 0x24FF, 4 },
    { 0x2776, 0x2793, 4 },
    { 0x2C00, 0x2DFF, 4 },
    { 0x2E80, 0x2FFF, 4 },

    { 0x3004, 0x3007, 5 },
    { 0x3021, 0x302F, 5 },
    { 0x3031, 0x303F, 5 },

    { 0x3040, 0xD7FF, 6 },

    { 0xF900, 0xFD3D, 7 },
    { 0xFD40, 0xFDCF, 7 },
    { 0xFDF0, 0xFE44, 7 },
    { 0xFE47, 0xFFFD, 7 },

    { 0x10000, 0x1FFFD, 8 },
    { 0x20000, 0x2FFFD, 8 },
    { 0x30000, 0x3FFFD, 8 },
    { 0x40000, 0x4FFFD, 8 },
    { 0x50000, 0x5FFFD, 8 },
    { 0x60000, 0x6FFFD, 8 },
    { 0x70000, 0x7FFFD, 8 },
    { 0x80000, 0x8FFFD, 8 },
    { 0x90000, 0x9FFFD, 8 },
    { 0xA0000, 0xAFFFD, 8 },
    { 0xB0000, 0xBFFFD, 8 },
    { 0xC0000, 0xCFFFD, 8 },
    { 0xD0000, 0xDFFFD, 8 },
    { 0xE0000, 0xEFFFD, 8 },
};

// D.2 Ranges of characters disallowed initially
constexpr uint32_t ucn_initial_disallowed[][3] = {
    { 0x0300, 0x036F, 1 },
    { 0x1DC0, 0x1DFF, 1 },
    { 0x20D0, 0x20FF, 1 },
    { 0xFE20, 0xFE2F, 1 },
};

bool binary_range_search(std::uint32_t n, const std::uint32_t table[][3], size_t size) {
    int l = 0;
    int h = static_cast<int>(size);
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

inline
bool is_valid_ucn_for_identifier(std::uint32_t n) {
    return binary_range_search(n, ucn_allowed, size(ucn_allowed));
}

inline
bool is_valid_ucn_for_identifier_initially(std::uint32_t n) {
    return !binary_range_search(n, ucn_initial_disallowed, size(ucn_initial_disallowed));
}

inline
bool is_valid_ucn(std::uint32_t n) {
    if ((n < 0xA0) && (n != 0x24 && n != 0x40 && n != 0x60)) {
        return false;
    }
    if (0xD800 <= n && n <= 0xDFFF) {
        return false;
    }
    if (0x10FFFF < n) {
        return false;
    }

    return true;
}

bool contains_invalid_ucn_for_identifier(const std::string& s) {
    constexpr string::size_type kUcnDigitStart = 2;
    constexpr string::size_type kUcn16Digits = 4;
    constexpr string::size_type kUcn32Digits = 8;

    string::size_type i = 0;
    string::size_type digits = 0;
    bool ucn;
    if (s[0] != '\\') {
        ++i;
    } else {
        switch (s[1]) {
        case 'u':
            digits = kUcn16Digits;
            ucn = true;
            break;

        case 'U':
            digits = kUcn32Digits;
            ucn = true;
            break;

        default:
            assert(false);
            ucn = false;
            break;
        }

        if (ucn) {
            uint32_t n = 0;
            auto r = from_chars(&s[kUcnDigitStart], &s[kUcnDigitStart + digits], n, 16);
            if (!is_valid_ucn_for_identifier_initially(n)) {
                return true;
            }
            i += kUcnDigitStart + digits;
        } else {
            ++i;
        }
    }

    while (i < s.length()) {
        if (s[i] != '\\') {
            ++i;
        } else {
            switch (s[i + 1]) {
            case 'u':
                digits = kUcn16Digits;
                ucn = true;
                break;

            case 'U':
                digits = kUcn32Digits;
                ucn = true;
                break;

            default:
                ucn = false;
                break;
            }

            if (ucn) {
                uint32_t n = 0;
                auto r = from_chars(&s[i + kUcnDigitStart], &s[i + kUcnDigitStart + digits], n, 16);
                if (!is_valid_ucn_for_identifier(n)) {
                    return true;
                }
                i += kUcnDigitStart + digits;
            } else {
                ++i;
            }
        }
    }

    return false;
}

inline
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

Scanner::Scanner(std::istream& input, bool trigraph)
    : input_(input)
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
    , ucn_digit_start_() {
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
    if (c_ == EOF) {
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
                if (c_ == '\n') {
                    cseq_ += to_c(c_);
                    c_ = get();
                    finish(TokenType::kNewLine);
                } else if (c_ == '\r') {
                    transit(ScannerState::kLineBreak, c_);
                } else {
                    transit(ScannerState::kWhiteSpaces, c_);
                }
                break;

            case kDigit:
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188, c_);
                break;

            case kLetter:
                if (c_ == 'L') {
                    transit(ScannerState::kState232_225_244_264_249_234_269, c_);
                } else if (c_ == 'U') {
                    transit(ScannerState::kState242_262_232_249_234_269_223, c_);
                } else if (c_ == 'u') {
                    transit(ScannerState::kState256_257_260_249_232_234_236_269_237_240_217_218_221, c_);
                } else {
                    transit(ScannerState::kState232_234_227, c_);
                }
                break;

            case '_':
                transit(ScannerState::kState232_234_227, c_);
                break;

            case '\\':
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
                cseq_ += to_c(c_);
                c_ = get();
                finish(TokenType::kNonWhiteSpaceCharacter);
                break;
            }

            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188: {
            mark();
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D"
            }
            break;
        }
        case ScannerState::kState86_88_89_158_159: {
            mark();
            if (c_ == ':') {
                transit(ScannerState::kState90, c_);
            } else if (c_ == '>') {
                transit(ScannerState::kState160, c_);
            } else {
                finish(TokenType::kPunctuator); // ":"
            }
            break;
        }
        case ScannerState::kState256_257_260_249_232_234_236_269_237_240_217_218_221: {
            mark();
            if (c_ == '"') {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == '\'') {
                transit(ScannerState::kState250_251, c_);
            } else if (c_ == '8') {
                transit(ScannerState::kState258_232_249_234_219_269_238, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "u"
            }
            break;
        }
        case ScannerState::kState80_81_74_142_143: {
            mark();
            if (c_ == '=') {
                transit(ScannerState::kState144, c_);
            } else if (c_ == '|') {
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
            if (c_ == '=') {
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
            if (c_ == '+' || c_ == '-') {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState193_194_189_190: {
            if (isdigit(c_)) {  // 'D'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_188_191, c_);
            } else if (c_ == '_' || isalpha(c_)) {  // 'N'
                transit(ScannerState::kState192_195_196_198_200_202_204_205_206_209_211_212_213_214_186_188, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState68_69_38: {
            mark();
            if (c_ == '=') {
                transit(ScannerState::kState70, c_);
            } else {
                finish(TokenType::kPunctuator); // "!"
            }
            break;
        }
        case ScannerState::kState103_102_30: {
            mark();
            if (c_ == '=') {
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
            if (c_ == '+' || c_ == '-') {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState28_134_135_76_77: {
            mark();
            if (c_ == '&') {
                transit(ScannerState::kState78, c_);
            } else if (c_ == '=') {
                transit(ScannerState::kState136, c_);
            } else {
                finish(TokenType::kPunctuator); // "&"
            }
            break;
        }
        case ScannerState::kState258_232_249_234_219_269_238: {
            mark();
            if (c_ == '"') {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == '\'') {
                transit(ScannerState::kState250_251, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                finish(TokenType::kIdentifier); // "u8"
            }
            break;
        }
        case ScannerState::kState16_17_34_118_119_24_25: {
            mark();
            if (c_ == '-') {
                transit(ScannerState::kState26, c_);
            } else if (c_ == '=') {
                transit(ScannerState::kState120, c_);
            } else if (c_ == '>') {
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
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
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
            if (c_ == ':') {
                transit(ScannerState::kState176_177_172, c_);
            } else if (c_ == '=') {
                transit(ScannerState::kState112, c_);
            } else if (c_ == '>') {
                transit(ScannerState::kState168, c_);
            } else {
                finish(TokenType::kPunctuator); // "%"
            }
            break;
        }
        case ScannerState::kState201_207: {
            if (c_ == '+' || c_ == '-') {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState273_274_270_271: {
            if (c_ == '"') {
                transit(ScannerState::kState275, c_);
            } else if (c_ == '\\') {    // 's'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState272_274_271);
            } else {    // 's'
                if (is_eol(c_)) {
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
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188);
            } else {
                finish(TokenType::kPpNumber);   // "D..."
            }
            break;
        }
        case ScannerState::kState72_138_139: {
            mark();
            if (c_ == '=') {
                transit(ScannerState::kState140, c_);
            } else {
                finish(TokenType::kPunctuator); // "^"
            }
            break;
        }
        case ScannerState::kState128_129_48_49_54_60_61: {
            mark();
            if (c_ == '=') {
                transit(ScannerState::kState62, c_);
            } else if (c_ == '>') {
                transit(ScannerState::kState130_131_50, c_);
            } else {
                finish(TokenType::kPunctuator); // ">"
            }
            break;
        }
        case ScannerState::kState182_184_94_14_95: {
            mark();
            if (c_ == '.') {
                transit(ScannerState::kState96_97, c_);
            } else if (isdigit(c_)) {   // 'D'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188, c_);
            } else {
                finish(TokenType::kPunctuator); // "."
            }
            break;
        }
        case ScannerState::kState96_97: {
            if (c_ == '.') {
                transit(ScannerState::kState98, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188: {
            mark();
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
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
            if (c_ == '#') {
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
            if (c_ == '"') {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == '\'') {
                transit(ScannerState::kState250_251, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
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
            if (c_ == '"') {
                transit(ScannerState::kState273_274_270_271, c_);
            } else if (c_ == '\'') {
                transit(ScannerState::kState250_251, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
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
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
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
            if (isdigit(c_) || c_ == '_' || isalpha(c_)) {  // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                error = contains_invalid_ucn_for_identifier(cseq_);
                finish(TokenType::kIdentifier); // "S"
            }
            break;
        }
        case ScannerState::kState250_251: {
            if (c_ == '\\') {   // 'c'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState251_252_253);
            } else {    // 'c'
                if (is_eol(c_)) {
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
            if (c_ == '+') {
                transit(ScannerState::kState22, c_);
            } else if (c_ == '=') {
                transit(ScannerState::kState116, c_);
            } else {
                finish(TokenType::kPunctuator); // "+"
            }
            break;
        }
        case ScannerState::kState40_106_107: {
            mark();
            if (c_ == '=') {
                transit(ScannerState::kState108, c_);
            } else if (c_ == '/') {
                transit(ScannerState::kLineComment, c_);
            } else if (c_ == '*') {
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
            if (c_ == '%') {
                transit(ScannerState::kState164, c_);
            } else if (c_ == ':') {
                transit(ScannerState::kState156, c_);
            } else if (c_ == '<') {
                transit(ScannerState::kState124_125_46, c_);
            } else if (c_ == '=') {
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
            if (c_ == '%') {
                transit(ScannerState::kState178_179, c_);
            } else {
                finish(TokenType::kHash);    // "%:"
            }
            break;
        }
        case ScannerState::kState203_207: {
            if (c_ == '+' || c_ == '-') {   // 'g'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188, c_);
            } else if (c_ == '\\') {
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
            if (isdigit(c_) || c_ == '_' || isalpha(c_)) {  // 'C'
                transit(ScannerState::kState232_233, c_);
            } else if (c_ == '\\') {
                transit(ScannerState::kStateUniversalCharacterNameInitial, c_, ScannerState::kState232_233);
            } else {
                error = contains_invalid_ucn_for_identifier(cseq_);
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
            if (c_ == '=') {
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
            if (c_ == '\'') {
                transit(ScannerState::kState193_194_189_190, c_);
            } else if (c_ == '.') {
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188, c_);
            } else if (c_ == 'E') {
                transit(ScannerState::kState207_199, c_);
            } else if (c_ == 'P') {
                transit(ScannerState::kState203_207, c_);
            } else if (c_ == 'e') {
                transit(ScannerState::kState197_207, c_);
            } else if (c_ == 'p') {
                transit(ScannerState::kState201_207, c_);
            } else if (isdigit(c_) || c_ == '_' || isalpha(c_)) {   // 'C'
                transit(ScannerState::kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188, c_);
            } else if (c_ == '\\') {
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
            if (c_ == '=') {
                transit(ScannerState::kState132, c_);
            } else {
                finish(TokenType::kPunctuator); // ">>"
            }
            break;
        }
        case ScannerState::kState272_274_271: {
            if (c_ == '"') {
                transit(ScannerState::kState275, c_);
            } else if (c_ == '\\') {    // 's'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState272_274_271);
            } else {    // 's'
                if (is_eol(c_)) {
                    error = true;
                } else {
                    // XXX: is_member_of_source_char_set(c_)
                    transit(ScannerState::kState272_274_271, c_);
                }
            }
            break;
        }
        case ScannerState::kState251_252_253: {
            if (c_ == '\'') {
                transit(ScannerState::kState254, c_);
            } else if (c_ == '\\') {    // 'c'
                transit(ScannerState::kStateEscapeSequenceInitial, c_, ScannerState::kState251_252_253);
            } else {    // 'c'
                if (is_eol(c_)) {
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
            if (c_ == ':') {
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
            while (!is_eol(c_)) {
                cseq_ += to_c(c_);
                c_ = get();
            }

            finish(TokenType::kComment);
            break;
        }
        case ScannerState::kBlockComment: {
            int prev = '\0';
            while (true) {
                if (c_ == EOF) {
                    // error(Token(Token::kNullTokenValue, line, column), /* コメントが終了しなかった。 */);
                    break;
                }
                cseq_ += to_c(c_);
                if (c_ == '/' && prev == '*') {
                    c_ = get();
                    break;
                }
                prev = c_;
                c_ = get();
            }

            finish(TokenType::kComment);
            break;
        }
        case ScannerState::kWhiteSpaces: {
            while (is_ws(c_)) {
                cseq_ += to_c(c_);
                c_ = get();
            }

            finish(TokenType::kWhiteSpace);
            break;
        }
        case ScannerState::kLineBreak: {
            if (c_ == '\n') {
                cseq_ += to_c(c_);
                c_ = get();
            }

            finish(TokenType::kNewLine);
            break;
        }
        case ScannerState::kStateUniversalCharacterNameInitial: {
            if (c_ == 'U') {
                ucn_digit_start_ = cseq_.length() + 1;
                transit(ScannerState::kStateUniversalCharacterName16_17, c_);
            } else if (c_ == 'u') {
                ucn_digit_start_ = cseq_.length() + 1;
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

            uint32_t n;
            auto r = from_chars(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()], n, 16);
            error = !is_valid_ucn(n);

            finish();   // \uXXXX
            break;
        }
        case ScannerState::kStateUniversalCharacterName32: {
            mark();

            uint32_t n;
            auto r = from_chars(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()], n, 16);
            error = !is_valid_ucn(n);

            finish();   // \UXXXXXXXX
            break;
        }
        case ScannerState::kStateEscapeSequenceInitial: {
            switch (c_) {
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
                ucn_digit_start_ = cseq_.length() + 1;
                transit(ScannerState::kStateEscapeSequence42_43, c_);
                break;

            case 'U':
                ucn_digit_start_ = cseq_.length() + 1;
                transit(ScannerState::kStateEscapeSequence52_53, c_);
                break;

            case 'x':
                transit(ScannerState::kStateEscapeSequence38_39, c_);
                break;

            default:
                if (is_oct(c_)) {
                    transit(ScannerState::kStateEscapeSequence32_33_26_28_29, c_);
                } else {
                    error = true;
                }
                break;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence52_53: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence54_55, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence54_55: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence56_57, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence56_57: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence58_59, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence58_59: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence60_61, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence38_39: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence40_39, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence42_43: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence44_45, c_);
            } else {
                error = true;
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
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence46_47: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence48_49, c_);
            } else {
                error = true;
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
                error = true;
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

                uint32_t n;
                auto r = from_chars(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()], n, 16);
                error = !is_valid_ucn(n);
            } else {
                error = true;
                finish();
            }
            break;
        }
        case ScannerState::kStateEscapeSequence62_63: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence64_65, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence64_65: {
            if (is_hex(c_)) {
                transit(ScannerState::kStateEscapeSequence66_67, c_);
            } else {
                error = true;
            }
            break;
        }
        case ScannerState::kStateEscapeSequence66_67: {
            if (is_hex(c_)) {
                // \UXXXXXXXX
                finish(c_); // ScannerState::kStateEscapeSequence68);

                uint32_t n;
                auto r = from_chars(&cseq_[ucn_digit_start_], &cseq_[cseq_.length()], n, 16);
                error = !is_valid_ucn(n);
            } else {
                error = true;
                finish();
            }
            break;
        }
        case ScannerState::kStateHeaderNameInitial: {
            int close_char = (cseq_[0] == '<') ? '>' : '"';
            if (c_ == close_char) {
                transit(ScannerState::kStateHeaderNameA1, c_);
            } else {
                if (is_eol(c_)) {
                    // 行末まで読み取ったので、ヘッダー名としては不正で、このままエラーで返す。
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

int Scanner::getline(std::string& result) {
    istream& input = input_.get();

    int c = input.get();
    if (c == EOF) {
        return -1;
    }

    result.clear();
    do {
        result += to_c(c);

        if (c == '\r') {
            int c2 = input.get();

            if (c2 == EOF) {
                if (input.eof()) {
                    // 一旦、ここまでに読み取れた分を呼び出し元に返す。
                    break;
                } else {
                    // でなければエラー。
                    return -1;
                }
            } else if (c2 == '\n') {
                result += to_c(c2);
            } else {
                input.putback(to_c(c2));
            }
            break;
        }
        if (c == '\n') {
            break;
        }

        c = input.get();
        if (c == EOF && !input.eof()) {
            return -1;
        }
    } while (c != EOF);

    return 0;
}

int Scanner::readline() {
    istream& input = input_.get();

    if (input.eof()) {
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

    auto not_nl = find_if(physical_line.rbegin(), physical_line.rend(), [](const auto& c) { return !is_nl(c); });
    if (not_nl == physical_line.rend()) {
        // TODO: 改行で終わらないのでエラー。尚、空のファイルは初回の getlineで弾かれる。
    }
    if (not_nl != physical_line.rend()) {
        auto last_c = find_if(not_nl, physical_line.rend(), [](const auto& c) { return !is_ws(c); });
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

void Scanner::transit(ScannerState next_state, int c) {
    cseq_ += to_c(c);
    state_ = next_state;
    c_ = get();
}

void Scanner::transit(ScannerState next_state, int c, ScannerState return_state) {
    if (return_state_ != ScannerState::kInitial) {
        throw runtime_error(__func__);
    }

    cseq_ += to_c(c);
    state_ = next_state;
    return_state_ = return_state;
    c_ = get();
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

void Scanner::finish(int c) {
    if (return_state_ == ScannerState::kInitial) {
        throw runtime_error(__func__);
    }

    cseq_ += to_c(c);
    state_ = return_state_;
    return_state_ = ScannerState::kInitial;
    c_ = get();
}

void Scanner::mark() {
    buf_i_mark_ = buf_i_;
}

void Scanner::reset(std::string& cseq) {
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
