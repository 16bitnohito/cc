#include <preprocessor/diagnostics.h>

using namespace std;

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

}   //  namespace pp
