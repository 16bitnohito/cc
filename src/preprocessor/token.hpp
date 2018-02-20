#if !defined(PREPROCESSOR_TOKEN_HPP_)
#define PREPROCESSOR_TOKEN_HPP_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <preprocessor/config.hpp>


namespace pp {

/**
 *  プリプロセッシングトークン
 */
class Token {
public:
	enum Type {
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
		kDefine,
		kUndef,
		kIf,
		kIfdef,
		kIfndef,
		kElif,
		kElse,
		kEndif,
		kError,
		kLine,
		kPragma,

		//kOpDefined,
		//kOpStringize,
		//kOpConcat,

		kPlaceMarker,

		kNonReplacementTarget,

		kEndOfFile,
	};

	class TokenValue {
	public:
		TokenValue(const std::string& string, Type type)
			: string_(string)
			, type_(type) {
		}

		~TokenValue() {
		}

		const std::string& string() const { return string_; }
		Type type() const { return type_; }

	private:
		std::string string_;
		Type type_;
	};

	static const std::shared_ptr<Token::TokenValue> kTokenValueNull;
	static const std::shared_ptr<Token::TokenValue> kTokenValueASpace;

	static const char* type_to_string(Type type);

	template <typename Container>
	static std::string concat_string(const Container& tokens) {
		std::string result;

		for (const auto& t : tokens) {
			result += t.string();
		}
		return result;
	}

	static std::shared_ptr<TokenValue> make_value(const std::string& string, Token::Type type) {
		return std::make_shared<TokenValue>(string, type);
	}

	Token()
		: value_(kTokenValueNull)
		, line_()
		, column_() {
	}

	Token(const std::string& string, Type type)
		: value_(Token::make_value(string, type))
		, line_()
		, column_() {
	}

	Token(const std::string& string, Type type, uint32_t line, uint32_t column)
		: value_(Token::make_value(string, type))
		, line_(line)
		, column_(column) {
	}

	Token(std::shared_ptr<TokenValue> value, uint32_t line, uint32_t column)
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

	bool operator==(const Token& rhs) const {
		return type() == rhs.type() && string() == rhs.string();
	}

	bool operator!=(const Token& rhs) const {
		return !(*this == rhs);
	}

	const std::string& string() const {
		return value_->string();
	}

	Type type() const {
		return static_cast<Type>(value_->type());
	}

	uint32_t line() const {
		return line_;
	}

	void line(uint32_t value) {
		line_ = value;
	}

	uint32_t column() const {
		return column_;
	}

	bool is_eol() const {
		return type() == Token::kNewLine || type() == Token::kEndOfFile;
	}

	bool is_ws() const {
		return type() == Token::kWhiteSpace || type() == Token::kComment;
	}

	bool is_ws_nl() const {
		return type() == Token::kWhiteSpace || type() == Token::kComment || type() == Token::kNewLine;
	}

private:
	std::shared_ptr<TokenValue> value_;
	uint32_t line_;
	uint32_t column_;
};

using TokenList = std::vector<Token>;


}   //  namespace pp

#endif	//  PREPROCESSOR_TOKEN_HPP_
