#if !defined(PREPROCESSOR_CALCULATOR_HPP_)
#define PREPROCESSOR_CALCULATOR_HPP_

#include <cstdint>
#include <string>
#include <preprocessor/config.hpp>


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
	uint8_t precedence;
	uint8_t arity;
};

void init_calculator();
bool parse_int(const std::string& s, target_intmax_t* result);

}   //  namespace pp

#endif	//  PREPROCESSOR_CALCULATOR_HPP_
