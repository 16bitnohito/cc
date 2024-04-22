#ifndef CC_PREPROCESSOR_DIAGNOSTICS_H_
#define CC_PREPROCESSOR_DIAGNOSTICS_H_

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <iosfwd>

#include "preprocessor/pp_config.h"
#include "preprocessor/token.h"
#include "util/utility.h"

namespace pp {

class SourceFile;

enum class DiagLevel {
    kDebug,
    kInfo,
    kWarning,
    kError,
    kFatalError,
};

constexpr auto kMinDiagLevel = DiagLevel::kDebug;
constexpr auto kMaxDiagLevel = DiagLevel::kFatalError;

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
extern const StringView kOpHasCAttributeIdentifierUsageError;
extern const StringView kOpHasCAttributeNeedsAnAttribute;
extern const StringView kOpHasIncludeIdentifierUsageError;
extern const StringView kOpHasIncludeParameterTypeMismatchError;
extern const StringView kOpHasEmbedParameterTypeMismatchError;
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

extern const StringView kNoEmbedResourceIdentifierError;
extern const StringView kEmbedResoruceNotFoundError;
extern const StringView kEmbedResoruceOpeningFailureError;
extern const StringView kEmbedResoruceReadingFailureError;
extern const StringView kEmbedUsingDefinedInLimitParameterError;
extern const StringView kEmbedLimitParameterLessThan0Error;
extern const StringView kEmbedResourceWidthCanBeDividedByEmbedElementWidth;
extern const StringView kBadEmbedParameterError;
extern const StringView kBadPrefixedEmbedParameterError;
extern const StringView kSameEmbedParameterSpecifiedError;
extern const StringView kEmbedParameterClauseUnspecifiedError;

extern const StringView kUnclosedBracketError;
extern const StringView kConditionalInclusionOperatorUsageError;

extern const StringView kLineNeedsDecimalConstantError;
extern const StringView kLineOutOfRangeError;

extern const StringView kUnknownEscapeSequenceWarning;
extern const StringView kInvalidHexadecimalEscapeSequenceFormatError;
extern const StringView kInvalidUniversalCharacterNameCodePointError;
extern const StringView kInvalidUniversalCharacterNameFormatError;
extern const StringView kInvalidIdentifierStartError;
extern const StringView kInvalidIdentifierContinueError;
extern const StringView kUnclosedHeaderNameError;
extern const StringView kEmptyCharacterConstant;
extern const StringView kUnclosedCharacterConstant;
extern const StringView kUnclosedStringLiteral;

/**
 */
class Location {
public:
    static Location from_source(SourceFile* source, const Token& token);

    Location(uint32_t line, uint32_t column)
        : line_(line)
        , column_(column) {
    }

    ~Location() {
    }

    uint32_t line() const {
        return line_;
    }

    uint32_t column() const {
        return column_;
    }

private:
    uint32_t line_;
    uint32_t column_;
};

/**
 */
class Diagnostics {
public:
    explicit Diagnostics();
    Diagnostics(const Diagnostics&) = delete;
    ~Diagnostics();

    Diagnostics& operator=(const Diagnostics&) = delete;

    void set_output(std::ostream* output);

    int warning_count() const;
    int error_count() const;

    template <class... Args>
    void debug(SourceFile* source, const Location& location, StringView message, Args... args) {
        output_diagnostic(DiagLevel::kDebug, source, location, message, std::make_format_args(args...));
    }

    template <class... Args>
    void info(SourceFile* source, const Location& location, StringView message, Args... args) {
        output_diagnostic(DiagLevel::kInfo, source, location, message, std::make_format_args(args...));
    }

    template <class... Args>
    void warning(SourceFile* source, const Location& location, StringView message, Args... args) {
        output_diagnostic(DiagLevel::kWarning, source, location, message, std::make_format_args(args...));
        ++warning_count_;
    }

    template <class... Args>
    void error(SourceFile* source, const Location& location, StringView message, Args... args) {
        output_diagnostic(DiagLevel::kError, source, location, message, std::make_format_args(args...));
        ++error_count_;
    }

    template <class... Args>
    [[noreturn]]
    void fatal_error(SourceFile* source, const Location& location, StringView message, Args... args) {
        output_diagnostic(DiagLevel::kFatalError, source, location, message, std::make_format_args(args...));
        std::exit(EXIT_FAILURE);
    }

private:
    void output_diagnostic(
            DiagLevel tag,
            SourceFile* source, const Location& location,
            StringView format, const std::format_args& args);

    std::ostream* output_;
    int warning_count_;
    int error_count_;
};

}   // namespace pp

#endif  // CC_PREPROCESSOR_DIAGNOSTICS_H_
