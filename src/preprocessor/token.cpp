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

    CASE(TokenType::kLeftBracket);
    CASE(TokenType::kRightBracket);
    CASE(TokenType::kLeftParenthesis);
    CASE(TokenType::kRightParenthesis);
    CASE(TokenType::kLeftBrace);
    CASE(TokenType::kRightBrace);
    CASE(TokenType::kEllipsis);
    CASE(TokenType::kComma);
    CASE(TokenType::kHash);
    CASE(TokenType::kHashHash);

    default: return "<UNKNOWN>";
    }
#undef CASE
}

bool token_list_equal(const TokenList& lhs, const TokenList& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    auto l = lhs.begin();
    auto r = rhs.begin();
    for (; l != lhs.end(); ++l, ++r) {
        if (l->type() != r->type()) {
            return false;
        }

        switch (l->type()) {
        case TokenType::kLeftBracket:
        case TokenType::kRightBracket:
        case TokenType::kLeftBrace:
        case TokenType::kRightBrace:
        case TokenType::kHash:
        case TokenType::kHashHash:
            // これらについては、タイプが同じであればそれは同じトークンとみなす。
            break;

        default:
            if (l->string() != r->string()) {
                return false;
            }
            break;
        }
    }

    return true;
}

const Token kTokenNull("", TokenType::kNull);
const Token kTokenEndOfFile("", TokenType::kEndOfFile);

}   //  namespace pp
