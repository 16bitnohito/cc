#include <preprocessor/scanner.hpp>

#include <algorithm>
#include <cassert>
#include <preprocessor/diagnostics.hpp>
#include <preprocessor/utility.hpp>

using namespace std;


namespace pp {
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
	return isxdigit(c);
}

inline
bool is_dec(int c) {
	return isdigit(c);
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

bool binary_range_search(uint32_t n, const uint32_t table[][3], size_t size) {
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
bool is_valid_ucn_for_identifier(uint32_t n) {
	return binary_range_search(n, ucn_allowed, size(ucn_allowed));
}

inline
bool is_valid_ucn_for_identifier_initially(uint32_t n) {
	return !binary_range_search(n, ucn_initial_disallowed, size(ucn_initial_disallowed));
}

inline
bool is_valid_ucn(uint32_t n) {
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

}	//  anonymous namespace


Scanner::Scanner(std::istream* input, bool trigraph, bool newline /* = true */)
    : input_(input)
	, buf_()
	, buf_i_()
	, line_number_()
	, trigraph_(trigraph)
	, newline_(newline)
	, c_()
	//, cseq_()
    //, state_(kStateInitial)
	, hint_(kHintInitial)
	//, type_()
	, buf_i_mark_() {
	//  申し訳程度
	static constexpr char kUtf8Bom[] = "\xef\xbb\xbf";
	for (auto b : kUtf8Bom) {
		int p = input->peek();
		int q = (b & 0xff);
		if (p != q) {
			break;
		}
		input->get();
	}

	c_ = get();
}

Scanner::~Scanner() {
}

Token Scanner::next_token() {
	if (c_ == EOF) {
		return Token("", Token::kEndOfFile);
	}

	clear_mark();
	bool error = false;
	bool match = false;
	string cseq;
	Token::Type type = Token::kNull;
	State state = kStateInitial;
	uint32_t line_number = line_number_;
	uint32_t column = buf_i_;

	while (!match && !error) {
		switch (state) {
		case kStateInitial: {
			mark();

			const int k = char_kind(c_);
			switch (k) {
			case kWsChar:
				cseq += to_c(c_);
				if (c_ == '\n') {
					state = kStateEnd;
					type = Token::kNewLine;
					c_ = get();
				} else if (c_ == '\r') {
					state = kStateLineBreak;
				} else {
					state = kStateWhiteSpaces;
				}
				break;

			case kDigit:
				cseq += to_c(c_);
				state = kStatePpNumberF1;
				break;

			case kLetter:
				if (c_ == 'L' || c_ == 'U' || c_ == 'u') {
					cseq += to_c(c_);
					state = kStateCharacterConstant2;
				} else {
					cseq += to_c(c_);;
					state = kStateIdentifierF1;
				}
				break;

			case '_':
				cseq += to_c(c_);
				state = kStateIdentifierF1;
				break;

			case '\\':
				cseq += to_c(c_);
				state = kStateIdentifier1;
				break;

			case '\'':
				cseq += to_c(c_);
				state = kStateCharacterConstant1;
				break;

			case '\"':
				if (hint_ == kHintIncludeDirective) {
					cseq += to_c(c_);
					state = kStateHeaderName1;
				} else {
					cseq += to_c(c_);
					state = kStateStringLiteral1;
				}
				break;

			case '[':
			case ']':
			case '(':
			case ')':
			case '{':
			case '}':
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
				break;

			case '.':
				//  '..'
				//  END
				cseq += to_c(c_);
				state = kStatePpNumber1;
				break;

			case ',':
				//  END
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
				break;

			case '+':
				//  '+'
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStatePlus;
				break;

			case '-':
				//  '>'
				//  '-'
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateMinus;
				break;

			case '*':
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateAsterisk;
				break;

			case '/':
				//  '/'
				//  '*'
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateSlash1;
				break;

			case '%':
				//  '>'
				//  ':'
				//  ':%:'
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStatePercent1;
				break;

			case '=':
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateEqual1;
				break;

			case '<':
				//  ':'
				//  '%'
				//  '='
				//  '<'
				//  '<='
				//  END
				if (hint_ == kHintIncludeDirective) {
					cseq += to_c(c_);
					state = kStateHeaderName1;
				} else {
					cseq += to_c(c_);
					state = kStateLess1;
				}
				break;

			case '>':
				//  '='
				//  '>'
				//  '>='
				//  END
				cseq += to_c(c_);
				state = kStateGreater1;
				break;

			case '^':
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateCaret1;
				break;

			case '|':
				//  '='
				//  '|'
				//  END
				cseq += to_c(c_);
				state = kStateVerticalBar1;
				break;

			case '&':
				//  '='
				//  '&'
				//  END
				cseq += to_c(c_);
				state = kStateAmpersand1;
				break;

			case '~':
				//  END
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
				break;

			case '!':
				//  '='
				//  END
				cseq += to_c(c_);
				state = kStateExclamation1;
				break;

			case '?':
				//  END
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
				break;

			case ':':
				//  '>'
				//  END
				cseq += to_c(c_);
				state = kStateColon1;
				break;

			case ';':
				//  END
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
				break;

			case '#':
				//  '#'
				//  END
				cseq += to_c(c_);
				state = kStatePound1;
				break;

			default:
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kNonWhiteSpaceCharacter;
				c_ = get();
				break;
			}
			break;
		}
		case kStateHeaderName1: {
			c_ = get();
			const int k = char_kind(c_);
			const char close_char = (cseq[0] == '<') ? '>' : '"';
			if (k == close_char) {
				cseq += to_c(c_);
				state = kStateHeaderNameF1;
			} else {
				if (k == '\n') {
					//  行末まで読み取ったので、ヘッダー名としては不正で、このままエラーで返す。
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateHeaderName1;
				}
			}
			break;
		}
		case kStateHeaderNameF1: {
			state = kStateEnd;
			type = Token::kHeaderName;
			c_ = get();
			break;
		}
		case kStateIdentifier1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kLetter && (c_ == 'u' || c_ == 'U')) {
				cseq += to_c(c_);
				state = kStateIdentifier2;
			} else {
				//error = true;
				reset(cseq);
				type = Token::kNonWhiteSpaceCharacter;
				state = kStateEnd;
			}
			break;
		}
		case kStateIdentifier2: {
			//  UCN
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				const int d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = kStateIdentifier2;
				} else {
					state = kStateIdentifierF1;

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
		case kStateIdentifierF1: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '_' || k == kLetter || k == kDigit) {
				cseq += to_c(c_);
				state = kStateIdentifierF1;
			} else if (c_ == '\\') {
				cseq += to_c(c_);
				state = kStateIdentifier1;
			} else {
				type = Token::kIdentifier;
				state = kStateEnd;
			}
			break;
		}
		case kStatePpNumber1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kDigit) {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == '.') {
				cseq += to_c(c_);
				state = kStateEllipsis;
			} else {
				type = Token::kPunctuator;
				state = kStateEnd;
			}
			break;
		}
		case kStatePpNumber2: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == kLetter && (c_ == 'u' || c_ == 'U')) {
				cseq += to_c(c_);
				state = kStatePpNumber3;
			} else {
				error = true;
			}
			break;
		}
		case kStatePpNumber3: {
			//  UCN
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				const int d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = kStatePpNumber3;
				} else {
					state = kStatePpNumberF1;

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
		case kStatePpNumberF1: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == kLetter && (c_ == 'e' || c_ == 'E' || c_ == 'p' || c_ == 'P')) {
				cseq += to_c(c_);
				state = kStatePpNumberF2;
			} else if (k == '_' || k == kLetter) {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == kDigit) {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStatePpNumber2;
			} else {
				type = Token::kPpNumber;
				state = kStateEnd;
			}
			break;
		}
		case kStatePpNumberF2: {
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == kLetter && (c_ == 'e' || c_ == 'E' || c_ == 'p' || c_ == 'P')) {
				cseq += to_c(c_);
				state = kStatePpNumberF2;
			} else if (k == '_' || k == kLetter) {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == kDigit) {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == '+' || k == '-') {
				cseq += to_c(c_);
				state = kStatePpNumberF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStatePpNumber2;
			} else {
				type = Token::kPpNumber;
				state = kStateEnd;
			}
			break;
		}
		case kStateCharacterConstant1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				//  空の文字定数はエラー。
				cseq += to_c(c_);
				//state = kStateEnd;
				error = true;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateCharacterConstant4;
			} else {
				if (k == '\n') {
					reset(cseq);
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateCharacterConstant3;
				}
			}
			break;
		}
		case kStateCharacterConstant2: {
			//  case kStateStringLiteral2:
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				//  "\'"
				cseq += to_c(c_);
				state = kStateCharacterConstant1;
			} else if (k == '\"') {
				//  "u\""
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			} else if (cseq[0] == 'u' && (k == kDigit && c_ == '8')) {
				//  "u8\"" or identifier
				cseq += to_c(c_);
				state = kStateStringLiteral4;
			} else if ((k == '_' || k == kLetter || k == '\\') || k == kDigit) {
				//  "u."
				cseq += to_c(c_);
				state = kStateIdentifierF1;
			} else {
				//error = true;
				//  "u" only
				reset(cseq);
				type = Token::kIdentifier;
				state = kStateEnd;
			}
			break;
		}
		case kStateCharacterConstant3: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = kStateCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateCharacterConstant4;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateCharacterConstant3;
				}
			}
			break;
		}
		case kStateCharacterConstant4: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'' || k == '"' || k == '?' || k == '\\' ||
				(k == kLetter && (c_ == 'a' || c_ == 'b' || c_ == 'f' || c_ == 'n' || c_ == 'r' || c_ == 't' || c_ == 'v'))) {
				cseq += to_c(c_);
				state = kStateCharacterConstant3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateCharacterConstant5;
			} else if (k == kLetter && (c_ == 'U' || c_ == 'u')) {
				cseq += to_c(c_);
				state = kStateCharacterConstant6;
			} else if (k == kLetter && c_ == 'x') {
				cseq += to_c(c_);
				state = kStateCharacterConstant7;
			} else {
				reset(cseq);
				error = true;
			}
			break;
		}
		case kStateCharacterConstant5: {
			//  OCT
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = kStateCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateCharacterConstant4;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateCharacterConstant8;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateCharacterConstant3;
				}
			}
			break;
		}
		case kStateCharacterConstant6: {
			//  UCN
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				const int d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = kStateCharacterConstant6;
				} else {
					state = kStateCharacterConstant3;

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
		case kStateCharacterConstant7: {
			//  HEX
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = kStateCharacterConstant9;
			} else {
				error = true;
			}
			break;
		}
		case kStateCharacterConstant8: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = kStateCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateCharacterConstant4;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateCharacterConstant3;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateCharacterConstant3;
				}
			}
			break;
		}
		case kStateCharacterConstant9: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'') {
				cseq += to_c(c_);
				state = kStateCharacterConstantF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateCharacterConstant4;
			} else if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = kStateCharacterConstant9;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateCharacterConstant3;
				}
			}
			break;
		}
		case kStateCharacterConstantF1: {
			mark();
			c_ = get();
			type = Token::kCharacterConstant;
			state = kStateEnd;
			break;
		}
		case kStateStringLiteral1: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = kStateStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateStringLiteral3;
			} else {
				if (k == '\n') {
					reset(cseq);
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateStringLiteral1;
				}
			}
			break;
		}
		case kStateStringLiteral2: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			} else if (k == kDigit && c_ == '8') {
				cseq += to_c(c_);
				state = kStateStringLiteral4;
			} else {
				error = true;
			}
			break;
		}
		case kStateStringLiteral3: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\'' || k == '"' || k == '?' || k == '\\' ||
				(k == kLetter && (c_ == 'a' || c_ == 'b' || c_ == 'f' || c_ == 'n' || c_ == 'r' || c_ == 't' || c_ == 'v'))) {
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateStringLiteral5;
			} else if (k == kLetter && (c_ == 'U' || c_ == 'u')) {
				cseq += to_c(c_);
				state = kStateStringLiteral6;
			} else if (k == kLetter && (c_ == 'x')) {
				cseq += to_c(c_);
				state = kStateStringLiteral7;
			} else {
				//error = true;
				//  不正なエスケープシーケンスとして、そのまま続行する。
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			}
			break;
		}
		case kStateStringLiteral4: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				//  "u8\""
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			} else if ((k == '_' || k == kLetter || k == '\\') || k == kDigit) {
				//  "u8."
				cseq += to_c(c_);
				state = kStateIdentifierF1;
			} else {
				//  "u8"
				type = Token::kIdentifier;
				state = kStateEnd;
			}
			break;
		}
		case kStateStringLiteral5: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = kStateStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateStringLiteral3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateStringLiteral8;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateStringLiteral1;
				}
			}
			break;
		}
		case kStateStringLiteral6: {
			//  UCN
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				const int d = (cseq[1] == 'u') ? 6 : 10;
				cseq += to_c(c_);
				if (cseq.length() < d) {
					state = kStateStringLiteral6;
				} else {
					state = kStateStringLiteral1;

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
		case kStateStringLiteral7: {
			c_ = get();
			const int k = char_kind(c_);
			if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = kStateStringLiteral9;
			} else {
				error = true;
			}
			break;
		}
		case kStateStringLiteral8: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = kStateStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateStringLiteral3;
			} else if (k == kDigit && ('0' <= c_ && c_ <= '7')) {
				cseq += to_c(c_);
				state = kStateStringLiteral1;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateStringLiteral1;
				}
			}
			break;
		}
		case kStateStringLiteral9: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '\"') {
				cseq += to_c(c_);
				state = kStateStringLiteralF1;
			} else if (k == '\\') {
				cseq += to_c(c_);
				state = kStateStringLiteral3;
			} else if (isxdigit(c_)) {
				cseq += to_c(c_);
				state = kStateStringLiteral9;
			} else {
				if (k == '\n') {
					error = true;
				} else {
					cseq += to_c(c_);
					state = kStateStringLiteral1;
				}
			}
			break;
		}
		case kStateStringLiteralF1: {
			mark();
			c_ = get();
			type = Token::kStringLiteral;
			state = kStateEnd;
			break;
		}
		case kStateLineComment: {
			c_ = get();
			while (c_ != '\n' && c_ != EOF) {
				cseq += to_c(c_);
				c_ = get();
			}
			state = kStateEnd;
			type = Token::kComment;
			break;
		}
		case kStateBlockComment: {
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

			type = Token::kComment;
			c_ = get();
			state = kStateEnd;
			break;
		}
		case kStateWhiteSpaces: {
			c_ = get();

			if (is_ws(c_)) {
				cseq += to_c(c_);
				state = kStateWhiteSpaces;
			} else {
				type = Token::kWhiteSpace;
				state = kStateEnd;
			}
			break;
		}
		case kStateLineBreak: {
			c_ = get();

			if (c_ == '\n') {
				c_ = get();
			}
			cseq = '\n';
			type = Token::kNewLine;
			state = kStateEnd;
			break;
		}
		case kStateEllipsis: {
			c_ = get();
			const int k = char_kind(c_);
			if (k == '.') {
				//  "..."
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//error = true;
				reset(cseq);
				type = Token::kPunctuator;
				state = kStateEnd;
			}
			break;
		}
		case kStatePlus: {
			//  '+'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '+') {
				//  "++"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '=') {
				//  "+="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "+"
				type = Token::kPunctuator;
				state = kStateEnd;
			}
			break;
		}
		case kStateMinus: {
			//  '>'
			//  '-'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				//  "->"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '-') {
				//  "--"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '=') {
				//  "-="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "-"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateAsterisk: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "*="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "*"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateSlash1: {
			//  '/'
			//  '*'
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '/') {
				cseq += to_c(c_);
				state = kStateLineComment;
			} else if (k == '*') {
				cseq += to_c(c_);
				state = kStateBlockComment;
			} else if (k == '=') {
				//  "/="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "/"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStatePercent1: {
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
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == ':') {
				cseq += to_c(c_);
				state = kStatePercent2;
			} else if (k == '=') {
				//  "%="
				cseq += (c_ & 0xff);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "%"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStatePercent2: {
			//  '%:'
			//  END
			mark();

			c_ = get();
			const int k = char_kind(c_);
			if (k == '%') {
				cseq += to_c(c_);
				state = kStatePercent3;
			} else {
				//  "%:"
				cseq = "#";
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStatePercent3: {
			//  ':'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == ':') {
				//  "%:%:"
				cseq = "##";
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "%:"
				reset(cseq);
				cseq = "#";
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateEqual1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "=="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "="
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateLess1: {
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
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '%') {
				//  "<%"
				cseq = "{";
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '<') {
				cseq += to_c(c_);
				state = kStateLess2;
			} else if (k == '=') {
				//  "<="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "<"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateLess2: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "<<="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "<<"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateGreater1: {
			//  '>'
			//  '='
			//  '>='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				cseq += to_c(c_);
				state = kStateGreater2;
			} else if (k == '=') {
				//  ">="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  ">"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateGreater2: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  ">>="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  ">>"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateCaret1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "^="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "^"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateVerticalBar1: {
			//  '='
			//  '|'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "|="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '|') {
				//  "||"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "|"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateAmpersand1: {
			//  '='
			//  '&'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "&="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else if (k == '&') {
				//  "&&"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "&"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateExclamation1: {
			//  '='
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '=') {
				//  "!="
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "!"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateColon1: {
			//  '>'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '>') {
				//  ":>"
				cseq = "]";
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  ":"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStatePound1: {
			//  '#'
			//  END
			c_ = get();
			const int k = char_kind(c_);
			if (k == '#') {
				//  "##"
				cseq += to_c(c_);
				state = kStateEnd;
				type = Token::kPunctuator;
				c_ = get();
			} else {
				//  "#"
				state = kStateEnd;
				type = Token::kPunctuator;
			}
			break;
		}
		case kStateEnd: {
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
		type = Token::kNonWhiteSpaceCharacter;
	}

	return Token(cseq, type, line_number, column);
}

bool Scanner::is_support_trigraph() {
	return trigraph_;
}

void Scanner::state_hint(Hint hint) {
	hint_ = hint;
}

uint32_t Scanner::line_number() {
	return line_number_;
}

void Scanner::line_number(uint32_t value) {
	line_number_ = value;
}

uint32_t Scanner::column() {
	return buf_i_;
}

std::string Scanner::buffer() {
	return buf_;
}

int Scanner::readline() {
	if (input_->eof()) {
		return EOF;
	}

	int concat = 0;
	string s;
	buf_.clear();
	do {
		getline(*input_, s);
		if (input_->bad()) {
			//  XXX
			return EOF;
		}
		if (buf_.length() >= numeric_limits<decltype(buf_i_)>::max()) {
			//  XXX
			return EOF;
		}

		//  1.
		if (is_support_trigraph()) {
			replace_trigraphs(s);
		}

		//  2.
		if (concat > 0) {
			buf_.pop_back();	//  pop '\\'
		}

		//  TODO: こういう優しさが必要かもしれない。
		//auto found = find_if(s.rbegin(), s.rend(), [](auto c) {
		//	return !is_ws(c);
		//});
		//if (found != s.rend()) {
		//	if (found != s.rbegin() && *found == '\\') {
		//		s.erase(found.base(), s.end());
		//	}
		//}

		buf_ += s;
		concat++;
		line_number_++;
	} while (!buf_.empty() && buf_[buf_.length() - 1] == '\\');

	buf_i_ = 0;
	if (newline_) {
		buf_ += "\n";
	}

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

//void Scanner::transit(int c, State to_state) {
//	cseq_ += static_cast<char>(c);
//	state_ = to_state;
//}

//void Scanner::finish(Token::Type token_type) {
//	state_ = kStateEnd;
//	type_ = token_type;
//	c_ = get();
//}

void Scanner::mark() {
	buf_i_mark_ = buf_i_;
}

void Scanner::reset(std::string& cseq) {
	assert(buf_i_mark_ != 0);
	assert(buf_i_ > buf_i_mark_);

	uint32_t n = (buf_i_ - buf_i_mark_) - 1;
	cseq.erase(cseq.length() - n);
	buf_i_ = buf_i_mark_;
	c_ = get();
}

void Scanner::clear_mark() {
	buf_i_mark_ = 0;
}

}   //  namespace pp
