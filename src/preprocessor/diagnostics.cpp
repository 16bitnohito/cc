#include "preprocessor/diagnostics.h"

#include <format>
#include <iostream>

#include "util/utility.h"

using namespace lib::util;
using namespace std;

namespace {

const char* const kLevelTag[] = {
    "デバッグ",
    "情報",
    "警告",
    "エラー",
    "致命的エラー",
};

}   // namespace

namespace pp {

const StringView kPredefinedMacroNameError = T_("\"{}\"はマクロ名として利用できない。");
const StringView kMacroRedefinitionWarning = T_("マクロ {}が再定義された。(以前の定義は {}:{})");
const StringView kFuncTypeMacroUsageWarning = T_("関数型マクロが関数呼び出しの形になっていない。");
const StringView kMustBeMacroName = T_("マクロ名を指定しなければならない。");
const StringView kRedundantTokens = T_("余分なトークンが有る。");
const StringView kUndefineNondefinedMacroWarning = T_("定義されていないマクロ \"{}\"を未定義にしようとした。");
const StringView kOpPragmaUsedAsMacroName = T_("\"_Pragma\"はマクロ名として利用できない。");
const StringView kOpPragmaParameterTypeMismatch = T_("\"_Pragma\"演算子の引数は文字列リテラルでなければならない。");
const StringView kVaArgsIdentifierUsageError = T_("__VA_ARGS__は関数型マクロの可変長引数の展開にのみ利用できる。");
const StringView kVaOptIdentifierUsageError = T_("__VA_OPT__は可変長引数を取る関数型マクロでのみ利用できる。");
const StringView kOpHasCAttributeIdentifierUsageError = T_("__has_c_attributeはここでは使えない。");
const StringView kOpHasCAttributeNeedsAnAttribute = T_("__has_c_attributeには属性を 1つ指定しなければならない。");
const StringView kOpHasIncludeIdentifierUsageError = T_("__has_includeはここでは使えない。");
const StringView kOpHasIncludeParameterTypeMismatchError = T_("__has_includeの引数はヘッダー名として解釈できなければならない。");
const StringView kStdcReservedMacroName = T_("__STDC_で始まるマクロ名は将来のために予約されている。");
const StringView kStdcReservedIdentifierDoubleUnderscore = T_("2つのアンダースコアで始まる識別子は予約されている。");
const StringView kStdcReservedIdentifierUnderscoreAndUppercase = T_("アンダースコアに続いて 1つの大文字で始まる識別子は予約されている。");
const StringView kStdcReservedIdentifierUnderscore = T_("アンダースコアで始まる識別子は予約されている。");
const StringView kStdcReservedIdentifierE = T_("Eに続いて 1つの数字又は Eに続いて 1つの大文字で始まる識別子は、将来、errno.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierFe = T_("FE_に続いて 1つの大文字で始まる識別子は、将来、fenv.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierPriScn = T_("PRI又は SCNに続いて 1つの小文字又は Xが続く識別子は、将来、inttypes.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierLc = T_("LC_に続いて 1つの大文字で始まる識別子は、将来、locale.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierSig = T_("SIGに続いて 1つの大文字又は SIG_に続いて 1つの大文字で始まる識別子は、将来、signal.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierAtomic = T_("ATOMIC_に続いて 1つの大文字で始まる識別子は、将来、stdatomic.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierIntMax = T_("INT又は UINTで始まり _MAX又は _MINで終わる識別子は、将来、stdint.hに追加されるかもしれない。");
const StringView kStdcReservedIdentifierTime = T_("TIME_に続いて 1つの大文字で始まる識別子は、将来、time.hに追加されるかもしれない。");
const StringView kOpStringizeNeedsParameterError = T_("#演算子に対象となる仮引数の識別子が指定されていない。");
const StringView kOpConcatNeedsParameterError = T_("##演算子が置換リストの先頭または末尾にある。");
const StringView kGeneratedInvalidPpTokenError = T_("連結によって不正なトークンが生成された。");
const StringView kGeneratedInvalidPpTokenError2 = T_("\"{}\"と \"{}\"は連結されない。");
const StringView kBadElipsisError = T_("マクロの可変長引数は最後になければならない。");
const StringView kBadMacroParameterFormError = T_("マクロの仮引数が識別子とコンマの並びになっていない。");
const StringView kSameMacroParameterIdError = T_("マクロの仮引数の識別子が重複してはならない。");
const StringView kBadMacroArgumentError = T_("マクロの引数リストが閉じていない。");
const StringView kUnmatchedNumberOfArguments = T_("マクロに渡された引数の数({})が仮引数の数({})と合わない。");
const StringView kVaArgsRequiresAtLeastOneArgument = T_("可変引数は少なくとも 1つの引数を必要とする。");

const StringView kInvalidConstantExpressionError = T_("整数定数式でなければならない。");
const StringView kConstantNumberIsNotAIntegerError = T_("{}は整数ではないか、ここでは取り扱えない。");
const StringView kInvalidOperatorError = T_("\"{}\"は定数式で使えない演算子である。");

const StringView kNoCorrespondingIfError = T_("#ifの無い {}が見つかった。");
const StringView kUnterminatedIfError = T_("{}行目の #ifに対応する #endifが無い。");
const StringView kElifGroupAfterElseError = T_("#elseの後に {}が見つかった。");
const StringView kIdsAreEvaluatedToZero = T_("識別子 {}は 0に評価される。");
const StringView kInvalidMacroName = T_("マクロ名は識別子でなければならない。");
const StringView kInvalidHeaderName = T_("ヘッダー名が不正である。");
const StringView kPragmaIsIgnoredWarning = T_("プラグマ {}は無視される。");
const StringView kNoIdentifierSpecifiedError = T_("{}の識別子が指定されていない。");

const StringView kMinSpecSourceFileInclusionWarning = T_("#includeの入れ子が {}回を超えた。({}回)");
const StringView kMinSpecConditionalInclusionWarning = T_("条件分岐の入れ子が {}回を超えた。({}回)");
const StringView kMinSpecMacroParametersWarning = T_("マクロのパラメーターが {}個を超えた。({}個)");
const StringView kMinSpecMacroArgumentsWarning = T_("マクロの引数が {}個を超えた。({}個)");

const StringView kNoEmbedResourceIdentifierError = T_("リソース識別子が指定されていない。");
const StringView kEmbedResoruceNotFoundError = T_("リソース {} が見つからない。");
const StringView kEmbedResoruceOpeningFailureError = T_("リソース {} が開けない。");
const StringView kEmbedResoruceReadingFailureError = T_("リソース {} が読み取れない。");
const StringView kEmbedUsingDefinedInLimitParameterError = T_("limitの定数式に definedは使えない。");
const StringView kEmbedLimitParameterLessThan0Error = T_("limitは 0以上でなければならない。");
const StringView kEmbedResourceWidthCanBeDividedByEmbedElementWidth = T_("リソースのサイズ（ビット数）は {}で割り切れなければならない。");
const StringView kBadEmbedParameterError = T_("パラメーター名が無い。");
const StringView kBadPrefixedEmbedParameterError = T_("接頭辞付きパラメーター名にならない。");
const StringView kSameEmbedParameterSpecifiedError = T_("{}は 0または 1つだけ指定できる。");
const StringView kEmbedParameterClauseUnspecifiedError = T_("{}のパラメーター句が無い。");

const StringView kUnclosedBracketError = T_("括弧が閉じていない。");
const StringView kConditionalInclusionOperatorUsageError = T_("識別子 {}はここでは使えない。");

const StringView kLineNeedsDecimalConstantError = T_("#lineには 10進整数（接尾辞無し）を指定しなければならない。");
const StringView kLineOutOfRangeError = T_("#lineに指定する行数は [{}, {}]の範囲でなければならない。");

const StringView kUnknownEscapeSequenceWarning = T_("エスケープシーケンスとして認識されない。");
const StringView kInvalidUniversalCharacterNameError = T_("ユニバーサル文字名で指定できないコードポイントである。");


Diagnostics::Diagnostics()
    : output_()
    , error_count_()
    , warning_count_() {
}

Diagnostics::~Diagnostics() {
}

void Diagnostics::set_output(std::ostream* output) {
    output_ = output;
}

int Diagnostics::warning_count() const {
    return warning_count_;
}

int Diagnostics::error_count() const {
    return error_count_;
}

void Diagnostics::output_diagnostic(
        DiagLevel level,
        SourceFile* source, const Token& token,
        StringView format, const std::format_args& args) {
    if (!output_) {
        throw runtime_error(__func__);
    }

    if (level < kMinDiagLevel || level > kMaxDiagLevel) {
        throw invalid_argument("level");
    }

    uint32_t l;
    size_t c;
    if (token.type() != TokenType::kNull) {
        l = token.line();
        c = token.column();
    } else {
        if (source) {
            l = source->line();
            c = source->column();
        } else {
            l = 0;
            c = 0;
        }
    }

    string s;
    if (source) {
        s = source_from_internal(source->source_path());
    } else {
        s = source_from_internal(T_("<init>"));
    }

    auto i = enum_ordinal(level);

    //ErrorOutputIterator it(*error_output_);
    //format_to(it, "{}:{}:{}: {}: ", s, l, c, kLevelTag[i]);
    //vformat_to(it, format, args);
    //format_to(it, "\n");
    auto log = std::format("{}:{}:{}: {}: {}\n", s, l, c, kLevelTag[i], vformat(format, args));
    output_->write(log.data(), log.size());
    output_->flush();
}


}   //  namespace pp
