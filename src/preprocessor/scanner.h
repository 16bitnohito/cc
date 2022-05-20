#ifndef PREPROCESSOR_SCANNER_H_
#define PREPROCESSOR_SCANNER_H_

#include <cinttypes>
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
    explicit Scanner(std::istream& input, bool trigraph);
    Scanner(const Scanner&) = delete;
    ~Scanner();

    Scanner& operator=(const Scanner&) = delete;

    Token next_token();

    bool is_support_trigraph();
    void state_hint(ScannerHint hint);

    std::uint32_t line_number();
    void line_number(std::uint32_t value);
    std::uint32_t column();

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
    bool splice_source_line(std::string& logical_line, std::string& physical_line);
    int getline(std::string& result);
    int readline();
    //void transit(int c, State to_state);
    //void finish(Token::Type token_type);
    void mark();
    void reset(std::string& cseq);
    void clear_mark();

    std::reference_wrapper<std::istream> input_;
    std::string buf_;
    std::uint32_t buf_i_;
    std::uint32_t line_number_;
    bool trigraph_;
    int c_;
    //std::string cseq_;
    //State state_;
    ScannerHint hint_;
    //Token::Type type_;
    std::uint32_t buf_i_mark_;
};

}   //  namespace pp

#endif	//  PREPROCESSOR_SCANNER_H_
