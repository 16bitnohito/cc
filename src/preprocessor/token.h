#ifndef PREPROCESSOR_TOKEN_H_
#define PREPROCESSOR_TOKEN_H_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <preprocessor/config.h>


namespace pp {

enum class TokenType {
    kNull = 256,

    kHeaderName,
    kIdentifier,
    kPpNumber,
    kCharacterConstant,
    kStringLiteral,
    kPunctuator,
    kNonWhiteSpaceCharacter,

    kNewLine,
    kWhiteSpace,
    kComment,

    kInclude,
    kEmbed,
    kDefine,
    kUndef,
    kIf,
    kIfdef,
    kIfndef,
    kElif,
    kElifdef,
    kElifndef,
    kElse,
    kEndif,
    kError,
    kWarning,
    kLine,
    kPragma,

    //kOpDefined,
    //kOpStringize,
    //kOpConcat,

    kPlaceMarker,

    kNonReplacementTarget,

    kEndOfFile,

    // kPunctuatorで代替トークンが有るものはタイプを明示する。
    kLeftBracket,
    kRightBracket,
    kLeftBrace,
    kRightBrace,
    kHash,
    kHashHash,

    // ついでに、コード中でよく利用される kPunctuatorのトークンも明示する。
    kLeftParenthesis,
    kRightParenthesis,
    kEllipsis,
    kComma,
};

/**
 *  プリプロセッシングトークン
 */
class Token {
public:
    class TokenValue {
    public:
        TokenValue(const std::string& string, TokenType type)
            : string_(string)
            , type_(type) {
        }

        ~TokenValue() {
        }

        const std::string& string() const { return string_; }
        TokenType type() const { return type_; }

    private:
        std::string string_;
        TokenType type_;
    };

    static const std::shared_ptr<Token::TokenValue> kTokenValueNull;
    static const std::shared_ptr<Token::TokenValue> kTokenValueASpace;

    static const char* type_to_string(TokenType type);

    template <typename Container>
    static std::string concat_string(const Container& tokens) {
        std::string result;

        for (const auto& t : tokens) {
            result += t.string();
        }
        return result;
    }

    static std::shared_ptr<TokenValue> make_value(const std::string& string, TokenType type) {
        return std::make_shared<TokenValue>(string, type);
    }

    Token()
        : value_(kTokenValueNull)
        , line_()
        , column_() {
    }

    Token(const std::string& string, TokenType type)
        : value_(Token::make_value(string, type))
        , line_()
        , column_() {
    }

    Token(const std::string& string, TokenType type, std::uint32_t line, std::uint32_t column)
        : value_(Token::make_value(string, type))
        , line_(line)
        , column_(column) {
    }

    Token(std::shared_ptr<TokenValue> value, std::uint32_t line, std::uint32_t column)
        : value_(value)
        , line_(line)
        , column_(column) {
    }

    Token(const Token& init) = default;
    Token(Token&& init) = default;

    ~Token() {
    }

    Token& operator=(const Token& rhs) = default;
    Token& operator=(Token&& rhs) = default;

    const std::string& string() const {
        return value_->string();
    }

    TokenType type() const {
        return value_->type();
    }

    std::uint32_t line() const {
        return line_;
    }

    void line(std::uint32_t value) {
        line_ = value;
    }

    std::uint32_t column() const {
        return column_;
    }

    bool is_null() const {
        return type() == TokenType::kNull;
    }

    bool is_eol() const {
        return type() == TokenType::kNewLine || type() == TokenType::kEndOfFile;
    }

    bool is_ws() const {
        return type() == TokenType::kWhiteSpace || type() == TokenType::kComment;
    }

    bool is_ws_nl() const {
        return type() == TokenType::kWhiteSpace || type() == TokenType::kComment || type() == TokenType::kNewLine;
    }

private:
    std::shared_ptr<TokenValue> value_;
    std::uint32_t line_;
    std::uint32_t column_;
};

using TokenList = std::vector<Token>;
bool token_list_equal(const TokenList& lhs, const TokenList& rhs);

extern const Token kTokenNull;
extern const Token kTokenEndOfFile;
extern const Token kTokenPpNumberZero;
extern const Token kTokenPpNumberOne;
extern const Token kTokenPpNumberTwo;

inline
bool is_bracket(TokenType type) {
    switch (type) {
    case TokenType::kLeftParenthesis:
    case TokenType::kLeftBracket:
    case TokenType::kLeftBrace:
    case TokenType::kRightParenthesis:
    case TokenType::kRightBracket:
    case TokenType::kRightBrace:
        return true;
    default:
        return false;
    }
}

inline
bool is_left_bracket(TokenType type) {
    switch (type) {
    case TokenType::kLeftParenthesis:
    case TokenType::kLeftBracket:
    case TokenType::kLeftBrace:
        return true;
    default:
        return false;
    }
}

inline
TokenType get_right_bracket(TokenType type) {
    switch (type) {
    case TokenType::kLeftParenthesis:
        return TokenType::kRightParenthesis;

    case TokenType::kLeftBracket:
        return TokenType::kRightBracket;

    case TokenType::kLeftBrace:
        return TokenType::kLeftBrace;

    default:
        return TokenType::kNull;
    }
}

inline
bool is_matched_bracket(TokenType left, TokenType right) {
    return (left == TokenType::kLeftParenthesis && right == TokenType::kRightParenthesis) ||
        (left == TokenType::kLeftBracket && right == TokenType::kRightBracket) ||
        (left == TokenType::kLeftBrace && right == TokenType::kRightBrace);
}


}   // namespace pp

#endif  // PREPROCESSOR_TOKEN_H_
