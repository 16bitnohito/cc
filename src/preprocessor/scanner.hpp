#if !defined(PREPROCESSOR_SCANNER_HPP_)
#define PREPROCESSOR_SCANNER_HPP_

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <preprocessor/config.hpp>
#include <preprocessor/token.hpp>


namespace pp {

//constexpr char kWarning[] = "行末のバックスラッシュの後に余分な空白が有る。";
//constexpr char kError[] = "文字定数が閉じていない。";
//constexpr char kError[] = "文字列リテラルが閉じていない。";
//constexpr char kError[] = "整数として正しくない。";
//constexpr char kError[] = "実数として正しくない。";

/**
*/
class Scanner {
public:
	enum Hint {
		kHintInitial,
		kHintIncludeDirective,
	};

	Scanner(std::istream* input, bool trigraph, bool newline = true);
    ~Scanner();

    Token next_token();

    bool is_support_trigraph();
    void state_hint(Hint hint);

	uint32_t line_number();
	void line_number(uint32_t value);
	uint32_t column();
	std::string buffer();

private:
	enum State {
		kStateInitial,
		kStateHeaderName1,
		kStateHeaderNameF1,
		kStateIdentifier1,
		kStateIdentifier2,
		kStateIdentifierF1,
		kStatePpNumber1,
		kStatePpNumber2,
		kStatePpNumber3,
		kStatePpNumberF1,
		kStatePpNumberF2,
		kStateCharacterConstant1,
		kStateCharacterConstant2,
		kStateCharacterConstant3,
		kStateCharacterConstant4,
		kStateCharacterConstant5,
		kStateCharacterConstant6,
		kStateCharacterConstant7,
		kStateCharacterConstant8,
		kStateCharacterConstant9,
		kStateCharacterConstantF1,
		kStateStringLiteral1,
		kStateStringLiteral2,
		kStateStringLiteral3,
		kStateStringLiteral4,
		kStateStringLiteral5,
		kStateStringLiteral6,
		kStateStringLiteral7,
		kStateStringLiteral8,
		kStateStringLiteral9,
		kStateStringLiteralF1,
		kStateLineComment,
		kStateBlockComment,
		kStateWhiteSpaces,
		kStateLineBreak,
		kStateEllipsis,
		kStatePlus,
		kStateMinus,
		kStateAsterisk,
		kStateSlash1,
		kStatePercent1,
		kStatePercent2,
		kStatePercent3,
		kStateEqual1,
		kStateLess1,
		kStateLess2,
		kStateGreater1,
		kStateGreater2,
		kStateCaret1,
		kStateVerticalBar1,
		kStateAmpersand1,
		kStateExclamation1,
		kStateColon1,
		kStatePound1,
		kStateEnd,
	};

	int get() {
		if (buf_i_ >= buf_.length()) {
			if (readline() != 0) {
				return EOF;
			}
		}

		int c = buf_[buf_i_] & 0xff;
		buf_i_++;

		return c;
	}

	std::string replace_trigraphs(std::string& s);
	int readline();
	//void transit(int c, State to_state);
	//void finish(Token::Type token_type);
	void mark();
	void reset(std::string& cseq);
	void clear_mark();

	std::istream* input_;
    std::string buf_;
	uint32_t buf_i_;
	uint32_t line_number_;
    bool trigraph_;
	bool newline_;
	int c_;
	//std::string cseq_;
	//State state_;
	Hint hint_;
	//Token::Type type_;
	uint32_t buf_i_mark_;
};

}   //  namespace pp

#endif	//  PREPROCESSOR_SCANNER_HPP_
