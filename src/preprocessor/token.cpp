#include <preprocessor/token.h>

using namespace std;


namespace pp {

const std::shared_ptr<Token::TokenValue> Token::kTokenValueNull = make_shared<TokenValue>("", TokenType::kNull);
const std::shared_ptr<Token::TokenValue> Token::kTokenValueASpace = make_shared<TokenValue>(" ", TokenType::kWhiteSpace);

//  static
const char* Token::type_to_string(TokenType type) {
#define CASE(t)     case t: return #t
    switch (type) {
    CASE(TokenType::kHeaderName);
    CASE(TokenType::kIdentifier);
    CASE(TokenType::kPpNumber);
    CASE(TokenType::kCharacterConstant);
    CASE(TokenType::kStringLiteral);
    CASE(TokenType::kPunctuator);
    CASE(TokenType::kNonWhiteSpaceCharacter);

    CASE(TokenType::kNewLine);
    CASE(TokenType::kWhiteSpace);
    CASE(TokenType::kComment);

    CASE(TokenType::kInclude);
    CASE(TokenType::kDefine);
    CASE(TokenType::kUndef);
    CASE(TokenType::kIf);
    CASE(TokenType::kIfdef);
    CASE(TokenType::kIfndef);
    CASE(TokenType::kElif);
    CASE(TokenType::kElifdef);
    CASE(TokenType::kElifndef);
    CASE(TokenType::kElse);
    CASE(TokenType::kEndif);
    CASE(TokenType::kError);
    CASE(TokenType::kWarning);
    CASE(TokenType::kLine);
    CASE(TokenType::kPragma);

    //CASE(TokenType::kOpDefined);
    //CASE(TokenType::kOpStringize);
    //CASE(TokenType::kOpConcat);

    CASE(TokenType::kPlaceMarker);

    CASE(TokenType::kNonReplacementTarget);

    CASE(TokenType::kEndOfFile);

    default: return "<UNKNOWN>";
    }
#undef CASE
}

const Token kTokenNull("", TokenType::kNull);
const Token kTokenEndOfFile("", TokenType::kEndOfFile);

}   //  namespace pp
