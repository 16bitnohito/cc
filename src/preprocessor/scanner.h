#ifndef PREPROCESSOR_SCANNER_H_
#define PREPROCESSOR_SCANNER_H_

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <preprocessor/config.h>
#include <preprocessor/token.h>


namespace pp {

//constexpr char kWarning[] = "行末のバックスラッシュの後に余分な空白が有る。";
//constexpr char kError[] = "文字定数が閉じていない。";
//constexpr char kError[] = "文字列リテラルが閉じていない。";
//constexpr char kError[] = "整数として正しくない。";
//constexpr char kError[] = "実数として正しくない。";


/**
 */
enum class ScannerHint {
	kInitial,
	kIncludeDirective,
};


/**
*/
class Scanner {
public:
	Scanner(std::istream* input, bool trigraph, bool newline = true);
    ~Scanner();

    Token next_token();

    bool is_support_trigraph();
    void state_hint(ScannerHint hint);

	uint32_t line_number();
	void line_number(uint32_t value);
	uint32_t column();
	std::string buffer();

private:
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
	ScannerHint hint_;
	//Token::Type type_;
	uint32_t buf_i_mark_;
};

}   //  namespace pp

#endif	//  PREPROCESSOR_SCANNER_H_
