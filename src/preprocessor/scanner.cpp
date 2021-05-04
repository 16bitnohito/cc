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

	return true;
}

inline
char to_c(int c) {
	return static_cast<char>(c);
}

enum class ScannerState {
	kInitial,
	kHeaderName1,
	kHeaderNameF1,
	kIdentifier1,
	kIdentifier2,
	kIdentifierF1,
	kPpNumber1,
	kPpNumber2,
	kPpNumber3,
	kPpNumberF1,
	kPpNumberF2,
	kCharacterConstant1,
	kCharacterConstant2,
	kCharacterConstant3,
	kCharacterConstant4,
	kCharacterConstant5,
	kCharacterConstant6,
	kCharacterConstant7,
	kCharacterConstant8,
	kCharacterConstant9,
	kCharacterConstantF1,
	kStringLiteral1,
	kStringLiteral2,
	kStringLiteral3,
	kStringLiteral4,
	kStringLiteral5,
	kStringLiteral6,
	kStringLiteral7,
	kStringLiteral8,
	kStringLiteral9,
	kStringLiteralF1,
	kLineComment,
	kBlockComment,
	kWhiteSpaces,
	kLineBreak,
	kEllipsis,
	kPlus,
	kMinus,
	kAsterisk,
	kSlash1,
	kPercent1,
	kPercent2,
	kPercent3,
	kEqual1,
	kLess1,
	kLess2,
	kGreater1,
	kGreater2,
	kCaret1,
	kVerticalBar1,
	kAmpersand1,
	kExclamation1,
	kColon1,
	kPound1,
	kEnd,
};

std::string& to_upper_string(std::string& s, std::size_t pos = 0) {
	for (size_t i = pos; i < s.length(); i++) {
		if ('a' <= s[i] && s[i] <= 'z') {
			s[i] = 'A' + (s[i] - 'a');
		}
	}
	return s;
}

}	//  anonymous namespace

namespace pp {

Scanner::Scanner(std::istream& input, bool trigraph)
    : input_(input)
	, buf_()
	, buf_i_()
	, line_number_()
	, trigraph_(trigraph)
	, c_()
	//, cseq_()
    //, state_(ScannerState::kInitial)
	, hint_(ScannerHint::kInitial)
	//, type_()
	, buf_i_mark_() {
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
	bool match = false;
	string cseq;
	TokenType type = TokenType::kNull;
	ScannerState state = ScannerState::kInitial;
	uint32_t line_number = line_number_;
	uint32_t column = buf_i_;

	while (!match && !error) {
		switch (state) {
		case ScannerState::kInitial: {
			mark();

			const int k = char_kind(c_);
			switch (k) {
			case kWsChar:
				cseq += to_c(c_);
				if (c_ == '\n') {
					state = ScannerState::kEnd;
					type = TokenType::kNewLine;
					c_ = get();
				} else if (c_ == '\r') {
					state = ScannerState::kLineBreak;
				} else {
					state = ScannerState::kWhiteSpaces;
				}
				break;

			case kDigit:
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
				break;

			case kLetter:
				if (c_ == 'L' || c_ == 'U' || c_ == 'u') {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant2;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kIdentifierF1;
				}
				break;

			case '_':
				cseq += to_c(c_);
				state = ScannerState::kIdentifierF1;
				break;

			case '\\':
				cseq += to_c(c_);
				state = ScannerState::kIdentifier1;
				break;

			case '\'':
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant1;
				break;

			case '\"':
				if (hint_ == ScannerHint::kIncludeDirective) {
					cseq += to_c(c_);
					state = ScannerState::kHeaderName1;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kStringLiteral1;
				}
				break;

			case '[':
			case ']':
			case '(':
			case ')':
			case '{':
			case '}':
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
				break;

			case '.':
				//  '..'
				//  END
				cseq += to_c(c_);
				state = ScannerState::kPpNumber1;
				break;

			case ',':
				//  END
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
				break;

			case '+':
				//  '+'
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kPlus;
				break;

			case '-':
				//  '>'
				//  '-'
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kMinus;
				break;

			case '*':
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kAsterisk;
				break;

			case '/':
				//  '/'
				//  '*'
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kSlash1;
				break;

			case '%':
				//  '>'
				//  ':'
				//  ':%:'
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kPercent1;
				break;

			case '=':
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kEqual1;
				break;

			case '<':
				//  ':'
				//  '%'
				//  '='
				//  '<'
				//  '<='
				//  END
				if (hint_ == ScannerHint::kIncludeDirective) {
					cseq += to_c(c_);
					state = ScannerState::kHeaderName1;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kLess1;
				}
				break;

			case '>':
				//  '='
				//  '>'
				//  '>='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kGreater1;
				break;

			case '^':
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kCaret1;
				break;

			case '|':
				//  '='
				//  '|'
				//  END
				cseq += to_c(c_);
				state = ScannerState::kVerticalBar1;
				break;

			case '&':
				//  '='
				//  '&'
				//  END
				cseq += to_c(c_);
				state = ScannerState::kAmpersand1;
				break;

			case '~':
				//  END
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
				break;

			case '!':
				//  '='
				//  END
				cseq += to_c(c_);
				state = ScannerState::kExclamation1;
				break;

			case '?':
				//  END
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
				break;

			case ':':
				//  '>'
				//  END
				cseq += to_c(c_);
				state = ScannerState::kColon1;
				break;

			case ';':
				//  END
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
				break;

			case '#':
				//  '#'
				//  END
				cseq += to_c(c_);
				state = ScannerState::kPound1;
				break;

			default:
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kNonWhiteSpaceCharacter;
				c_ = get();
				break;
			}
			break;
		}
		case ScannerState::kHeaderName1: {
			c_ = get();
			const int k = char_kind(c_);
			int close_char = (cseq[0] == '<') ? '>' : '"';
			if (k == close_char) {
				cseq += to_c(c_);
				state = ScannerState::kHeaderNameF1;
			} else {
				if (is_nl(c_)) {
					//  行末まで読み取ったので、ヘッダー名としては不正で、このままエラーで返す。
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kHeaderName1;
				}
			}
			break;
		}
		case ScannerState::kHeaderNameF1: {
			state = ScannerState::kEnd;
			type = TokenType::kHeaderName;
			c_ = get();
			break;
		}
		case ScannerState::kIdentifier1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kLetter && (c_ == 'u' || c_ == 'U')) {
				cseq += to_c(c_);
				state = ScannerState::kIdentifier2;
			} else {
				//error = true;
				reset(cseq);
				type = TokenType::kNonWhiteSpaceCharacter;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kIdentifier2: {
			//  UCN
			c_ = get();
			if (isxdigit(c_)) {
				const size_t d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = ScannerState::kIdentifier2;
				} else {
					state = ScannerState::kIdentifierF1;

					cseq = to_upper_string(cseq, 2);

					size_t next;
					uint32_t n = stoul(&cseq[2], &next, 16);
					if (next != (cseq.length() - 2)) {
						error = true;
					}
					if (!is_valid_ucn_for_identifier(n)) {
						error = true;
					} else {
						if (cseq.length() == d) {
							if (!is_valid_ucn_for_identifier_initially(n)) {
								error = true;
							}
						}
					}
					//  上位でチェックすべきかも。
					error = error || !is_valid_ucn(n);
				}
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kIdentifierF1: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '_' || k == kLetter || k == kDigit) {
				cseq += to_c(c_);
				state = ScannerState::kIdentifierF1;
			} else if (c_ == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kIdentifier1;
			} else {
				type = TokenType::kIdentifier;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kPpNumber1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kDigit) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == '.') {
				cseq += to_c(c_);
				state = ScannerState::kEllipsis;
			} else {
				type = TokenType::kPunctuator;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kPpNumber2: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kLetter && (c_ == 'u' || c_ == 'U')) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumber3;
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kPpNumber3: {
			//  UCN
			c_ = get();
			if (isxdigit(c_)) {
				const size_t d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = ScannerState::kPpNumber3;
				} else {
					state = ScannerState::kPpNumberF1;

					size_t next;
					uint32_t n = stoul(&cseq[2], &next, 16);
					if (next != (cseq.length() - 2)) {
						error = true;
					}
					if (!is_valid_ucn_for_identifier(n)) {
						error = true;
					} else {
						if (cseq.length() == d) {
							if (!is_valid_ucn_for_identifier_initially(n)) {
								error = true;
							}
						}
					}
					//  上位でチェックすべきかも。
					error = error || !is_valid_ucn(n);
				}
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kPpNumberF1: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == kLetter && (c_ == 'e' || c_ == 'E' || c_ == 'p' || c_ == 'P')) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF2;
			} else if (k == '_' || k == kLetter) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == kDigit) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kPpNumber2;
			} else {
				type = TokenType::kPpNumber;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kPpNumberF2: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == kLetter && (c_ == 'e' || c_ == 'E' || c_ == 'p' || c_ == 'P')) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF2;
			} else if (k == '_' || k == kLetter) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == kDigit) {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == '+' || k == '-') {
				cseq += to_c(c_);
				state = ScannerState::kPpNumberF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kPpNumber2;
			} else {
				type = TokenType::kPpNumber;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kCharacterConstant1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				//  空の文字定数はエラー。
				cseq += to_c(c_);
				//state = ScannerState::kEnd;
				error = true;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant4;
			} else {
				if (is_nl(c_)) {
					reset(cseq);
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant3;
				}
			}
			break;
		}
		case ScannerState::kCharacterConstant2: {
			//  case ScannerState::kStringLiteral2:
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				//  "\'"
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant1;
			} else if (k == '\"') {
				//  "u\""
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			} else if (cseq[0] == 'u' && (k == kDigit && c_ == '8')) {
				//  "u8\"" or identifier
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral4;
			} else if ((k == '_' || k == kLetter || k == '\\') || k == kDigit) {
				//  "u."
				cseq += to_c(c_);
				state = ScannerState::kIdentifierF1;
			} else {
				//error = true;
				//  "u" only
				reset(cseq);
				type = TokenType::kIdentifier;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kCharacterConstant3: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant4;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant3;
				}
			}
			break;
		}
		case ScannerState::kCharacterConstant4: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'' || k == '"' || k == '?' || k == '\\' ||
				(k == kLetter && (c_ == 'a' || c_ == 'b' || c_ == 'f' || c_ == 'n' || c_ == 'r' || c_ == 't' || c_ == 'v'))) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant5;
			} else if (k == kLetter && (c_ == 'U' || c_ == 'u')) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant6;
			} else if (k == kLetter && c_ == 'x') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant7;
			} else {
				reset(cseq);
				error = true;
			}
			break;
		}
		case ScannerState::kCharacterConstant5: {
			//  OCT
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant4;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant8;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant3;
				}
			}
			break;
		}
		case ScannerState::kCharacterConstant6: {
			//  UCN
			c_ = get();
			if (isxdigit(c_)) {
				const size_t d = (cseq[1] == 'u') ? 6U : 10U;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = ScannerState::kCharacterConstant6;
				} else {
					state = ScannerState::kCharacterConstant3;

					size_t next;
					uint32_t n = stoul(&cseq[2], &next, 16);
					if (next != (cseq.length() - 2)) {
						error = true;
					}
					if (!is_valid_ucn_for_identifier(n)) {
						error = true;
					} else {
						if (cseq.length() == d) {
							if (!is_valid_ucn_for_identifier_initially(n)) {
								error = true;
							}
						}
					}
					//  上位でチェックすべきかも。
					error = error || !is_valid_ucn(n);
				}
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kCharacterConstant7: {
			//  HEX
			c_ = get();
			if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant9;
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kCharacterConstant8: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant4;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant3;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant3;
				}
			}
			break;
		}
		case ScannerState::kCharacterConstant9: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant4;
			} else if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = ScannerState::kCharacterConstant9;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kCharacterConstant3;
				}
			}
			break;
		}
		case ScannerState::kCharacterConstantF1: {
			mark();
			c_ = get();
			type = TokenType::kCharacterConstant;
			state = ScannerState::kEnd;
			break;
		}
		case ScannerState::kStringLiteral1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral3;
			} else {
				if (is_nl(c_)) {
					reset(cseq);
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kStringLiteral1;
				}
			}
			break;
		}
		case ScannerState::kStringLiteral2: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			} else if (k == kDigit && c_ == '8') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral4;
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kStringLiteral3: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'' || k == '"' || k == '?' || k == '\\' ||
				(k == kLetter && (c_ == 'a' || c_ == 'b' || c_ == 'f' || c_ == 'n' || c_ == 'r' || c_ == 't' || c_ == 'v'))) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral5;
			} else if (k == kLetter && (c_ == 'U' || c_ == 'u')) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral6;
			} else if (k == kLetter && (c_ == 'x')) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral7;
			} else {
				//error = true;
				//  不正なエスケープシーケンスとして、そのまま続行する。
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			}
			break;
		}
		case ScannerState::kStringLiteral4: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				//  "u8\""
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			} else if ((k == '_' || k == kLetter || k == '\\') || k == kDigit) {
				//  "u8."
				cseq += to_c(c_);
				state = ScannerState::kIdentifierF1;
			} else {
				//  "u8"
				type = TokenType::kIdentifier;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kStringLiteral5: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral8;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kStringLiteral1;
				}
			}
			break;
		}
		case ScannerState::kStringLiteral6: {
			//  UCN
			c_ = get();
			if (isxdigit(c_)) {
				const size_t d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = ScannerState::kStringLiteral6;
				} else {
					state = ScannerState::kStringLiteral1;

					size_t next;
					uint32_t n = stoul(&cseq[2], &next, 16);
					if (next != (cseq.length() - 2)) {
						error = true;
					}
					if (!is_valid_ucn_for_identifier(n)) {
						error = true;
					} else {
						if (cseq.length() == d) {
							if (!is_valid_ucn_for_identifier_initially(n)) {
								error = true;
							}
						}
					}
					//  上位でチェックすべきかも。
					error = error || !is_valid_ucn(n);
				}
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kStringLiteral7: {
			c_ = get();
			if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral9;
			} else {
				error = true;
			}
			break;
		}
		case ScannerState::kStringLiteral8: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral1;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kStringLiteral1;
				}
			}
			break;
		}
		case ScannerState::kStringLiteral9: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral3;
			} else if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = ScannerState::kStringLiteral9;
			} else {
				if (is_nl(c_)) {
					error = true;
				} else {
					cseq += to_c(c_);
					state = ScannerState::kStringLiteral1;
				}
			}
			break;
		}
		case ScannerState::kStringLiteralF1: {
			mark();
			c_ = get();
			type = TokenType::kStringLiteral;
			state = ScannerState::kEnd;
			break;
		}
		case ScannerState::kLineComment: {
			c_ = get();
			while (!is_nl(c_) && c_ != EOF) {
				cseq += to_c(c_);
				c_ = get();
			}
			state = ScannerState::kEnd;
			type = TokenType::kComment;
			break;
		}
		case ScannerState::kBlockComment: {
			//do {
			//	while (c_ != '*' && c_ != EOF) {
			//		c_ = get();
			//		cseq += to_c(c_);
			//	}
			//	c_ = get();
			//	cseq += to_c(c_);
			//} while (c_ != '/' && c_ != EOF);

			int prev = 0;
			while (true) {
				c_ = get();
				if (c_ == EOF) {
					//  error(Token(Token::kNullTokenValue, line, column), /* コメントが終了しなかった。 */);
					break;
				}
				cseq += to_c(c_);
				if (c_ == '/') {
					if (prev == '*') {
						break;
					}
				}
				prev = c_;
			}

			type = TokenType::kComment;
			c_ = get();
			state = ScannerState::kEnd;
			break;
		}
		case ScannerState::kWhiteSpaces: {
			c_ = get();

			if (is_ws(c_)) {
				cseq += to_c(c_);
				state = ScannerState::kWhiteSpaces;
			} else {
				type = TokenType::kWhiteSpace;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kLineBreak: {
			c_ = get();

			if (c_ == '\n') {
				cseq += to_c(c_);
				c_ = get();
			}
			type = TokenType::kNewLine;
			state = ScannerState::kEnd;
			break;
		}
		case ScannerState::kEllipsis: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				//  "..."
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//error = true;
				reset(cseq);
				type = TokenType::kPunctuator;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kPlus: {
			//  '+'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '+') {
				//  "++"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '=') {
				//  "+="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "+"
				type = TokenType::kPunctuator;
				state = ScannerState::kEnd;
			}
			break;
		}
		case ScannerState::kMinus: {
			//  '>'
			//  '-'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				//  "->"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '-') {
				//  "--"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '=') {
				//  "-="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "-"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kAsterisk: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "*="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "*"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kSlash1: {
			//  '/'
			//  '*'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '/') {
				cseq += to_c(c_);
				state = ScannerState::kLineComment;
			} else if (k == '*') {
				cseq += to_c(c_);
				state = ScannerState::kBlockComment;
			} else if (k == '=') {
				//  "/="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "/"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kPercent1: {
			//  '>'
			//  ':'
			//  ':%:'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				//  "%>"
				cseq = "}";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == ':') {
				cseq += to_c(c_);
				state = ScannerState::kPercent2;
			} else if (k == '=') {
				//  "%="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "%"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kPercent2: {
			//  '%:'
			//  END
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '%') {
				cseq += to_c(c_);
				state = ScannerState::kPercent3;
			} else {
				//  "%:"
				cseq = "#";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kPercent3: {
			//  ':'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == ':') {
				//  "%:%:"
				cseq = "##";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "%:"
				reset(cseq);
				cseq = "#";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kEqual1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "=="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "="
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kLess1: {
			//  ':'
			//  '%'
			//  '<'
			//  '='
			//  '<='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == ':') {
				//  "<:"
				cseq = "[";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '%') {
				//  "<%"
				cseq = "{";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '<') {
				cseq += to_c(c_);
				state = ScannerState::kLess2;
			} else if (k == '=') {
				//  "<="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "<"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kLess2: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "<<="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "<<"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kGreater1: {
			//  '>'
			//  '='
			//  '>='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				cseq += to_c(c_);
				state = ScannerState::kGreater2;
			} else if (k == '=') {
				//  ">="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  ">"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kGreater2: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  ">>="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  ">>"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kCaret1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "^="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "^"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kVerticalBar1: {
			//  '='
			//  '|'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "|="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '|') {
				//  "||"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "|"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kAmpersand1: {
			//  '='
			//  '&'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "&="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else if (k == '&') {
				//  "&&"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "&"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kExclamation1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "!="
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "!"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kColon1: {
			//  '>'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				//  ":>"
				cseq = "]";
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  ":"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kPound1: {
			//  '#'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '#') {
				//  "##"
				cseq += to_c(c_);
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
				c_ = get();
			} else {
				//  "#"
				state = ScannerState::kEnd;
				type = TokenType::kPunctuator;
			}
			break;
		}
		case ScannerState::kEnd: {
			match = true;
			break;
		}
		default: {
			error = true;
			break;
		}
		}
	}

	if (error) {
		type = TokenType::kNonWhiteSpaceCharacter;
	}

	return Token(cseq, type, line_number, column);
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
    const int kTrigraphComponents = 3;
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
            case '\\':
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
                r += 3;
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

//void Scanner::transit(int c, State to_state) {
//	cseq_ += static_cast<char>(c);
//	state_ = to_state;
//}

//void Scanner::finish(TokenType token_type) {
//	state_ = ScannerState::kEnd;
//	type_ = token_type;
//	c_ = get();
//}

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
