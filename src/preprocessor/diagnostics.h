#ifndef PREPROCESSOR_DIAGNOSTICS_H_
#define PREPROCESSOR_DIAGNOSTICS_H_

#include <cinttypes>
#include <cstdio>
#include <preprocessor/config.h>
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

extern const StringView kPredefinedMacroNameError;
extern const StringView kMacroRedefinitionWarning;
extern const StringView kFuncTypeMacroUsageWarning;
extern const StringView kMustBeMacroName;
extern const StringView kRedundantTokens;
extern const StringView kUndefineNondefinedMacroWarning;
extern const StringView kOpPragmaUsedAsMacroName;
extern const StringView kOpPragmaParameterTypeMismatch;
extern const StringView kVaArgsIdentifierUsageError;
extern const StringView kVaOptIdentifierUsageError;
extern const StringView kStdcReservedMacroName;
extern const StringView kStdcReservedIdentifierDoubleUnderscore;
extern const StringView kStdcReservedIdentifierUnderscoreAndUppercase;
extern const StringView kStdcReservedIdentifierUnderscore;
extern const StringView kStdcReservedIdentifierE;
extern const StringView kStdcReservedIdentifierFe;
extern const StringView kStdcReservedIdentifierPriScn;
extern const StringView kStdcReservedIdentifierLc;
extern const StringView kStdcReservedIdentifierSig;
extern const StringView kStdcReservedIdentifierAtomic;
extern const StringView kStdcReservedIdentifierIntMax;
extern const StringView kStdcReservedIdentifierTime;
extern const StringView kOpStringizeNeedsParameterError;
extern const StringView kOpConcatNeedsParameterError;
extern const StringView kGeneratedInvalidPpTokenError;
extern const StringView kGeneratedInvalidPpTokenError2;
extern const StringView kBadElipsisError;
extern const StringView kBadMacroParameterFormError;
extern const StringView kSameMacroParameterIdError;
extern const StringView kBadMacroArgumentError;
extern const StringView kUnmatchedNumberOfArguments;
extern const StringView kVaArgsRequiresAtLeastOneArgument;

extern const StringView kInvalidConstantExpressionError;
extern const StringView kConstantNumberIsNotAIntegerError;
extern const StringView kInvalidOperatorError;

extern const StringView kNoCorrespondingIfError;
extern const StringView kUnterminatedIfError;
extern const StringView kElifGroupAfterElseError;
extern const StringView kIdsAreEvaluatedToZero;
extern const StringView kInvalidMacroName;
extern const StringView kInvalidHeaderName;
extern const StringView kPragmaIsIgnoredWarning;
extern const StringView kPragmaIsIgnoredWarningF;
extern const StringView kNoIdentifierSpecifiedError;

constexpr int kMinSpecSourceFileInclusion = 15;
constexpr int kMinSpecConditionalInclusion = 63;
constexpr int kMinSpecMacroParameters = 127;
constexpr int kMinSpecMacroArguments = 127;

extern const StringView kMinSpecSourceFileInclusionWarning;
extern const StringView kMinSpecConditionalInclusionWarning;
extern const StringView kMinSpecMacroParametersWarning;
extern const StringView kMinSpecMacroArgumentsWarning;


#if defined(NDEBUG)
#define DEBUG(t, ...)
#define DEBUG_EXPR(expr)
#else
#define DEBUG(t, ...)		debug(t, __VA_ARGS__)
#define DEBUG_EXPR(expr)	expr
#endif


//  TODO: 出力先を合わせるためだけのものから脱却させる or die。
class Diagnostics {
};

}   //  namespace pp

#endif	//  PREPROCESSOR_DIAGNOSTICS_H_
