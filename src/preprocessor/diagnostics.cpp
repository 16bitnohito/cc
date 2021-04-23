#include <preprocessor/diagnostics.h>

using namespace std;

namespace pp {

const Char* const kPredefinedMacroNameError = T_("\"%s\"はマクロ名として利用できない。");
const Char* const kMacroRedefinitionWarning = T_("マクロ %sが再定義された。(以前の定義は %s:%" PRIu32 ")");
const Char* const kFuncTypeMacroUsageWarning = T_("関数型マクロが関数呼び出しの形になっていない。");
const Char* const kMustBeMacroName = T_("マクロ名を指定しなければならない。");
const Char* const kRedundantTokens = T_("余分なトークンが有る。");
const Char* const kUndefineNondefinedMacroWarning = T_("定義されていないマクロ \"%s\"を未定義にしようとした。");
const Char* const kOpPragmaUsedAsMacroName = T_("\"_Pragma\"はマクロ名として利用できない。");
const Char* const kOpPragmaParameterTypeMismatch = T_("\"_Pragma\"演算子の引数は文字列リテラルでなければならない。");
const Char* const kVaArgsIdentifierUsageError = T_("__VA_ARGS__は関数型マクロの可変長引数の展開にのみ利用できる。");
const Char* const kVaOptIdentifierUsageError = T_("__VA_OPT__は可変長引数を取る関数型マクロでのみ利用できる。");
const Char* const kStdcReservedMacroName = T_("__STDC_で始まるマクロ名は将来のために予約されている。");
const Char* const kStdcReservedIdentifierDoubleUnderscore = T_("2つのアンダースコアで始まる識別子は予約されている。");
const Char* const kStdcReservedIdentifierUnderscoreAndUppercase = T_("アンダースコアに続いて 1つの大文字で始まる識別子は予約されている。");
const Char* const kStdcReservedIdentifierUnderscore = T_("アンダースコアで始まる識別子は予約されている。");
const Char* const kStdcReservedIdentifierE = T_("Eに続いて 1つの数字又は Eに続いて 1つの大文字で始まる識別子は、将来、errno.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierFe = T_("FE_に続いて 1つの大文字で始まる識別子は、将来、fenv.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierPriScn = T_("PRI又は SCNに続いて 1つの小文字又は Xが続く識別子は、将来、inttypes.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierLc = T_("LC_に続いて 1つの大文字で始まる識別子は、将来、locale.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierSig = T_("SIGに続いて 1つの大文字又は SIG_に続いて 1つの大文字で始まる識別子は、将来、signal.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierAtomic = T_("ATOMIC_に続いて 1つの大文字で始まる識別子は、将来、stdatomic.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierIntMax = T_("INT又は UINTで始まり _MAX又は _MINで終わる識別子は、将来、stdint.hに追加されるかもしれない。");
const Char* const kStdcReservedIdentifierTime = T_("TIME_に続いて 1つの大文字で始まる識別子は、将来、time.hに追加されるかもしれない。");
const Char* const kOpStringizeNeedsParameterError = T_("#演算子に対象となる仮引数の識別子が指定されていない。");
const Char* const kOpConcatNeedsParameterError = T_("##演算子が置換リストの先頭または末尾にある。");
const Char* const kGeneratedInvalidPpTokenError = T_("連結によって不正なトークンが生成された。");
const Char* const kGeneratedInvalidPpTokenError2 = T_("\"%s\"と \"%s\"は連結されない。");
const Char* const kBadElipsisError = T_("マクロの可変長引数は最後になければならない。");
const Char* const kBadMacroParameterFormError = T_("マクロの仮引数が識別子とコンマの並びになっていない。");
const Char* const kSameMacroParameterIdError = T_("マクロの仮引数の識別子が重複してはならない。");
const Char* const kBadMacroArgumentError = T_("マクロの引数リストが閉じていない。");
const Char* const kUnmatchedNumberOfArguments = T_("マクロに渡された引数の数(%" PRIu32 ")が仮引数の数(%" PRIu32 ")と合わない。");
const Char* const kVaArgsRequiresAtLeastOneArgument = T_("可変引数は少なくとも 1つの引数を必要とする。");

const Char* const kInvalidConstantExpressionError = T_("整数定数式でなければならない。");
const Char* const kConstantNumberIsNotAIntegerError = T_("%sは整数ではないか、ここでは取り扱えない。");
const Char* const kInvalidOperatorError = T_("\"%s\"は定数式で使えない演算子である。");

const Char* const kElifWithoutIfError = T_("#ifの無い #elifが見つかった。");
const Char* const kElseWithoutIfError = T_("#ifの無い #elseが見つかった。");
const Char* const kEndifWithoutIfError = T_("#ifの無い #endifが見つかった。");
const Char* const kUnterminatedIfError = T_("%" PRIu32 "行目の #ifに対応する #endifが無い。");
const Char* const kElifAfterElseError = T_("#elseの後に #elifが見つかった。");
const Char* const kIdsAreEvaluatedToZero = T_("識別子 %sは 0に評価される。");
const Char* const kInvalidMacroName = T_("マクロ名は識別子でなければならない。");
const Char* const kInvalidHeaderName = T_("ヘッダー名が不正である。");
const Char* const kPragmaIsIgnoredWarning = T_("プラグマ %sは無視される。");


const Char* const kMinSpecSourceFileInclusionWarning = T_("#includeの入れ子が %d回を超えた。(%d回)");
const Char* const kMinSpecConditionalInclusionWarning = T_("条件分岐の入れ子が %d回を超えた。(%d回)");
const Char* const kMinSpecMacroParametersWarning = T_("マクロのパラメーターが %d個を超えた。(%d個)");
const Char* const kMinSpecMacroArgumentsWarning = T_("マクロの引数が %d個を超えた。(%d個)");

}   //  namespace pp
