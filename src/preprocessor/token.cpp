#include <preprocessor/token.hpp>

using namespace std;


namespace pp {

const std::shared_ptr<Token::TokenValue> Token::kTokenValueNull = make_shared<TokenValue>("", Token::kNull);
const std::shared_ptr<Token::TokenValue> Token::kTokenValueASpace = make_shared<TokenValue>(" ", Token::kWhiteSpace);

//  static
const char* Token::type_to_string(Type type) {
#define CASE(t)		case t: return #t
	switch (type) {
	CASE(kHeaderName);
	CASE(kIdentifier);
	CASE(kPpNumber);
	CASE(kCharacterConstant);
	CASE(kStringLiteral);
	CASE(kPunctuator);
	CASE(kNonWhiteSpaceCharacter);

	CASE(kNewLine);
	CASE(kWhiteSpace);
	CASE(kComment);

	CASE(kInclude);
	CASE(kDefine);
	CASE(kUndef);
	CASE(kIf);
	CASE(kIfdef);
	CASE(kIfndef);
	CASE(kElif);
	CASE(kElse);
	CASE(kEndif);
	CASE(kError);
	CASE(kLine);
	CASE(kPragma);

	//CASE(kOpDefined);
	//CASE(kOpStringize);
	//CASE(kOpConcat);

	CASE(kPlaceMarker);

	CASE(kNonReplacementTarget);

	CASE(kEndOfFile);

	default: return "<UNKNOWN>";
	}
#undef CASE
}

}   //  namespace pp
