#ifndef CC_PREPROCESSOR_CALCULATOR_H_
#define CC_PREPROCESSOR_CALCULATOR_H_

#include <cstdint>
#include <string>

#include "preprocessor/pp_config.h"

namespace pp {

enum class OperatorId {
    kUnknown,
    kCloseParen,
    kOpenParen,
    //kComma,
    kColon,
    kCond,
    kOr,
    kAnd,
    kBitOr,
    kBitXor,
    kBitAnd,
    kEq,
    kNeq,
    kLt,
    kGt,
    kLeq,
    kGeq,
    kShl,
    kShr,
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kPlus,
    kMinus,
    kCompl,
    kNot,
};


struct Operator {
    OperatorId id;
    std::string sequence;
    std::uint8_t precedence;
    std::uint8_t arity;
};

void init_calculator();
bool parse_int(const std::string& s, target_intmax_t* result);

}   // namespace pp

#endif  // CC_PREPROCESSOR_CALCULATOR_H_
