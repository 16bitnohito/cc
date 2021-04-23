#ifndef PREPROCESSOR_DIAGNOSTICS_H_
#define PREPROCESSOR_DIAGNOSTICS_H_

#include <cinttypes>
#include <cstdio>
#include <preprocessor/utility.h>

namespace pp {

enum class DiagLevel {
	kNoLog,
	kFatalError,
	kError,
	kWarning,
	kInfo,
	kTrace,
	kDebug,
};

constexpr auto kMinDiagLevel = DiagLevel::kNoLog;
constexpr auto kMaxDiagLevel = DiagLevel::kDebug;

extern const Char* const kPredefinedMacroNameError;
extern const Char* const kMacroRedefinitionWarning;
extern const Char* const kFuncTypeMacroUsageWarning;
extern const Char* const kMustBeMacroName;
extern const Char* const kRedundantTokens;
extern const Char* const kUndefineNondefinedMacroWarning;
extern const Char* const kOpPragmaUsedAsMacroName;
extern const Char* const kOpPragmaParameterTypeMismatch;
extern const Char* const kVaArgsIdentifierUsageError;
extern const Char* const kVaOptIdentifierUsageError;
extern const Char* const kStdcReservedMacroName;
extern const Char* const kStdcReservedIdentifierDoubleUnderscore;
extern const Char* const kStdcReservedIdentifierUnderscoreAndUppercase;
extern const Char* const kStdcReservedIdentifierUnderscore;
extern const Char* const kStdcReservedIdentifierE;
extern const Char* const kStdcReservedIdentifierFe;
extern const Char* const kStdcReservedIdentifierPriScn;
extern const Char* const kStdcReservedIdentifierLc;
extern const Char* const kStdcReservedIdentifierSig;
extern const Char* const kStdcReservedIdentifierAtomic;
extern const Char* const kStdcReservedIdentifierIntMax;
extern const Char* const kStdcReservedIdentifierTime;
extern const Char* const kOpStringizeNeedsParameterError;
extern const Char* const kOpConcatNeedsParameterError;
extern const Char* const kGeneratedInvalidPpTokenError;
extern const Char* const kGeneratedInvalidPpTokenError2;
extern const Char* const kBadElipsisError;
extern const Char* const kBadMacroParameterFormError;
extern const Char* const kSameMacroParameterIdError;
extern const Char* const kBadMacroArgumentError;
extern const Char* const kUnmatchedNumberOfArguments;
extern const Char* const kVaArgsRequiresAtLeastOneArgument;

extern const Char* const kInvalidConstantExpressionError;
extern const Char* const kConstantNumberIsNotAIntegerError;
extern const Char* const kInvalidOperatorError;

extern const Char* const kElifWithoutIfError;
extern const Char* const kElseWithoutIfError;
extern const Char* const kEndifWithoutIfError;
extern const Char* const kUnterminatedIfError;
extern const Char* const kElifAfterElseError;
extern const Char* const kIdsAreEvaluatedToZero;
extern const Char* const kInvalidMacroName;
extern const Char* const kInvalidHeaderName;
extern const Char* const kPragmaIsIgnoredWarning;

constexpr int kMinSpecSourceFileInclusion = 15;
constexpr int kMinSpecConditionalInclusion = 63;
constexpr int kMinSpecMacroParameters = 127;
constexpr int kMinSpecMacroArguments = 127;

extern const Char* const kMinSpecSourceFileInclusionWarning;
extern const Char* const kMinSpecConditionalInclusionWarning;
extern const Char* const kMinSpecMacroParametersWarning;
extern const Char* const kMinSpecMacroArgumentsWarning;


#if defined(NDEBUG)
#define DEBUG(t, ...)
#define DEBUG_EXPR(expr)
#else
#define DEBUG(t, ...)		debug(t, __VA_ARGS__)
#define DEBUG_EXPR(expr)	expr
#endif


//  TODO: 出力先を合わせるためだけのものから脱却させる or die。
class Diagnostics {
public:
	static void setup(FILE* error_output) {
		error_output_ = error_output;
	}

	static FILE* error_output_file() {
		return error_output_;
	}

private:
	static inline FILE* error_output_ = nullptr;
};

}   //  namespace pp

#endif	//  PREPROCESSOR_DIAGNOSTICS_H_
