#if !defined(PREPROCESSOR_CALCULATOR_HPP_)
#define PREPROCESSOR_CALCULATOR_HPP_

#include <cstdint>
#include <string>
#include <preprocessor/config.hpp>


namespace pp {

enum OperatorId {
	kOpUnknown,
	kOpCloseParen,
	kOpOpenParen,
	//kOpComma,
	kOpColon,
	kOpCond,
	kOpOr,
	kOpAnd,
	kOpBitOr,
	kOpBitXor,
	kOpBitAnd,
	kOpEq,
	kOpNeq,
	kOpLt,
	kOpGt,
	kOpLeq,
	kOpGeq,
	kOpShl,
	kOpShr,
	kOpAdd,
	kOpSub,
	kOpMul,
	kOpDiv,
	kOpMod,
	kOpPlus,
	kOpMinus,
	kOpCompl,
	kOpNot,
};


struct Operator {
	OperatorId id;
	std::string sequence;
	uint8_t precedence;
	uint8_t arity;
};


bool parse_int(const std::string& s, target_intmax_t* result);

}   //  namespace pp

#endif	//  PREPROCESSOR_CALCULATOR_HPP_
