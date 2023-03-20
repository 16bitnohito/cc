#ifndef CC_PREPROCESSOR_SCANNER_H_
#define CC_PREPROCESSOR_SCANNER_H_

#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "preprocessor/pp_config.h"
#include "preprocessor/diagnostics.h"
#include "preprocessor/sourcefilestack.h"
#include "preprocessor/token.h"

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
    kHeaderName,
};

/**
 */
enum class ScannerState {
    kInitial,

    kState192_196_198_200_202_204_205_206_209_211_212_213_214_215_185_186_188,
    kState86_88_89_158_159,
    kState256_257_260_249_232_234_236_269_237_240_217_218_221,
    kState80_81_74_142_143,
    kState10,
    kState64_65_100,
    kState160,
    kState36,
    kState207_199,
    kState193_194_189_190,
    kState68_69_38,
    kState103_102_30,
    kState90,
    kState197_207,
    kState28_134_135_76_77,
    kState258_232_249_234_219_269_238,
    kState16_17_34_118_119_24_25,
    kState8,
    kState82,
    kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_188_191,
    kState92,
    kState166_167_170_42_171_174_110_175_111,
    kState201_207,
    kState273_274_270_271,
    kState136,
    kState112,
    kState26,
    kState146,
    kState192_196_198_200_202_204_205_206_209_210_211_212_213_214_186_188,
    kState72_138_139,
    kState128_129_48_49_54_60_61,
    kState182_184_94_14_95,
    kState96_97,
    kState192_196_198_200_202_204_205_206_209_211_212_213_214_186_187_188,
    kState70,
    kState148_150_151,
    kState275,
    kState232_225_244_264_249_234_269,
    kState12,
    kState242_262_232_249_234_269_223,
    kState4,
    kState144,
    kState192_195_196_198_200_202_204_205_206_209_211_212_213_214_186_188,
    kState140,
    kState232_234_227,
    kState250_251,
    kState32_114_115_20_21,
    kState40_106_107,
    kState2,
    kState104,
    kState84,
    kState162_163_122_123_44_45_52_56_57_154_155,
    kState164,
    kState98,
    kState152,
    kState6,
    kState168,
    kState176_177_172,
    kState203_207,
    kState62,
    kState156,
    kState232_233,
    kState66,
    kState124_125_46,
    kState116,
    kState120,
    kState78,
    kState192_196_198_200_202_204_205_206_208_209_211_212_213_214_186_188,
    kState18,
    kState108,
    kState130_131_50,
    kState272_274_271,
    kState251_252_253,
    kState58,
    kState22,
    kState132,
    kState254,
    kState126,
    kState178_179,
    kState180,

    kLineComment,
    kBlockComment,
    kWhiteSpaces,
    kLineBreak,

    kStateUniversalCharacterNameInitial,
    kStateUniversalCharacterName33_13_1,
    kStateUniversalCharacterName2_3_14_15,
    kStateUniversalCharacterName16_17,
    kStateUniversalCharacterName4_5,
    kStateUniversalCharacterName18_19,
    kStateUniversalCharacterName20_21,
    kStateUniversalCharacterName6_7,
    kStateUniversalCharacterName22_23,
    kStateUniversalCharacterName24_25,
    kStateUniversalCharacterName26_27,
    kStateUniversalCharacterName8_9,
    kStateUniversalCharacterName10_11,
    kStateUniversalCharacterName28_29,
    kStateUniversalCharacterName30_31,
    kStateUniversalCharacterName12,
    kStateUniversalCharacterName32,
    kStateEscapeSequenceInitial,
    kStateEscapeSequence52_53,
    kStateEscapeSequence54_55,
    kStateEscapeSequence56_57,
    kStateEscapeSequence58_59,
    kStateEscapeSequence38_39,
    kStateEscapeSequence42_43,
    kStateEscapeSequence40_39,
    kStateEscapeSequence44_45,
    kStateEscapeSequence46_47,
    kStateEscapeSequence32_33_26_28_29,
    kStateEscapeSequence60_61,
    kStateEscapeSequence34_35_30,
    kStateEscapeSequence48_49,
    kStateEscapeSequence62_63,
    kStateEscapeSequence64_65,
    kStateEscapeSequence66_67,
    kStateHeaderNameInitial,
    kStateHeaderNameA1,
};

/**
 */
class Scanner {
public:
    enum class Char32 : char32_t;
    //using Char32 = char32_t;
    using Char32String = std::basic_string<Char32>;

    explicit Scanner(std::istream& input, bool trigraph, Diagnostics& diag, SourceFileStack& sources);
    Scanner(const Scanner&) = delete;
    ~Scanner();

    Scanner& operator=(const Scanner&) = delete;

    Token next_token();

    bool is_support_trigraph();
    void state_hint(ScannerHint hint);

    std::uint32_t line_number();
    void line_number(std::uint32_t value);
    std::uint32_t column();

    bool eof() const;

private:
    Char32 get();
    std::string replace_trigraphs(std::string& s);
    bool splice_source_line(std::string& logical_line, std::string& physical_line);
    int getline(std::string& result);
    int readline();
    void consume(Char32 c);
    void transit(ScannerState next_state, Char32 c);
    void transit(ScannerState next_state, Char32 c, ScannerState return_state);
    void finish(TokenType token_type);
    void finish();
    void finish(Char32 c);
    void mark();
    void reset(Char32String& cseq);
    void clear_mark();

    std::istream& input_;
    Diagnostics& diag_;
    SourceFileStack& sources_;
    std::string buf_;
    std::uint32_t buf_i_;
    std::uint32_t line_number_;
    bool trigraph_;
    Char32 c_;
    std::string cseq_;
    ScannerState state_;
    ScannerState return_state_;
    ScannerHint hint_;
    TokenType type_;
    bool match_;
    std::uint32_t buf_i_mark_;
    std::uint32_t ucn_digit_start_;
    bool eof_;
};

}   // namespace pp

#endif  // CC_PREPROCESSOR_SCANNER_H_
