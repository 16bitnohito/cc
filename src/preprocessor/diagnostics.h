#if !defined(PREPROCESSOR_DIAGNOSTICS_HPP_)
#define PREPROCESSOR_DIAGNOSTICS_HPP_

#include <cinttypes>
#include <cstdio>


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

constexpr char kPredefinedMacroNameError[] = "\"%s\"はマクロ名として利用できない。";
constexpr char kMacroRedefinitionWarning[] = "マクロ %sが再定義された。(以前の定義は %s:%" PRIu32 ")";
constexpr char kFuncTypeMacroUsageWarning[] = "関数型マクロが関数呼び出しの形になっていない。";
constexpr char kMustBeMacroName[] = "マクロ名を指定しなければならない。";
constexpr char kRedundantTokens[] = "余分なトークンが有る。";
constexpr char kUndefineNondefinedMacroWarning[] = "定義されていないマクロ \"%s\"を未定義にしようとした。";
constexpr char kOpPragmaUsedAsMacroName[] = "\"_Pragma\"はマクロ名として利用できない。";
constexpr char kOpPragmaParameterTypeMismatch[] = "\"_Pragma\"演算子の引数は文字列リテラルでなければならない。";
constexpr char kVaArgsIdentifierUsageError[] = "__VA_ARGS__は関数型マクロの可変長引数の展開にのみ利用できる。";
constexpr char kVaOptIdentifierUsageError[] = "__VA_OPT__は可変長引数を取る関数型マクロでのみ利用できる。";
constexpr char kStdcReservedMacroName[] = "__STDC_で始まるマクロ名は将来のために予約されている。";
constexpr char kOpStringizeNeedsParameterError[] = "#演算子に対象となる仮引数の識別子が指定されていない。";
constexpr char kOpConcatNeedsParameterError[] = "##演算子が置換リストの先頭または末尾にある。";
constexpr char kGeneratedInvalidPpTokenError[] = "連結によって不正なトークンが生成された。";
constexpr char kGeneratedInvalidPpTokenError2[] = "\"%s\"と \"%s\"は連結されない。";
constexpr char kBadElipsisError[] = "マクロの可変長引数は最後になければならない。";
constexpr char kBadMacroParameterFormError[] = "マクロの仮引数が識別子とコンマの並びになっていない。";
constexpr char kSameMacroParameterIdError[] = "マクロの仮引数の識別子が重複してはならない。";
constexpr char kBadMacroArgumentError[] = "マクロの引数リストが閉じていない。";
constexpr char kUnmatchedNumberOfArguments[] = "マクロに渡された引数の数(%" PRIu32 ")が仮引数の数(%" PRIu32 ")と合わない。";
constexpr char kVaArgsRequiresAtLeastOneArgument[] = "可変引数は少なくとも 1つの引数を必要とする。";

constexpr char kInvalidConstantExpressionError[] = "整数定数式でなければならない。";
constexpr char kConstantNumberIsNotAIntegerError[] = "%sは整数ではないか、ここでは取り扱えない。";
constexpr char kInvalidOperatorError[] = "\"%s\"は定数式で使えない演算子である。";

constexpr char kElifWithoutIfError[] = "#ifの無い #elifが見つかった。";
constexpr char kElseWithoutIfError[] = "#ifの無い #elseが見つかった。";
constexpr char kEndifWithoutIfError[] = "#ifの無い #endifが見つかった。";
constexpr char kUnterminatedIfError[] = "%" PRIu32 "行目の #ifに対応する #endifが無い。";
constexpr char kElifAfterElseError[] = "#elseの後に #elifが見つかった。";
constexpr char kIdsAreEvaluatedToZero[] = "識別子 %sは 0に評価される。";
constexpr char kInvalidMacroName[] = "マクロ名は識別子でなければならない。";
constexpr char kInvalidHeaderName[] = "ヘッダー名が不正である。";
constexpr char kPragmaIsIgnoredWarning[] = "プラグマ %sは無視される。";

constexpr int kMinSpecSourceFileInclusion = 15;
constexpr int kMinSpecConditionalInclusion = 63;
constexpr int kMinSpecMacroParameters = 127;
constexpr int kMinSpecMacroArguments = 127;

constexpr char kMinSpecSourceFileInclusionWarning[] = "#includeの入れ子が %d回を超えた。(%d回)";
constexpr char kMinSpecConditionalInclusionWarning[] = "条件分岐の入れ子が %d回を超えた。(%d回)";
constexpr char kMinSpecMacroParametersWarning[] = "マクロのパラメーターが %d個を超えた。(%d個)";
constexpr char kMinSpecMacroArgumentsWarning[] = "マクロの引数が %d個を超えた。(%d個)";


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

#endif	//  PREPROCESSOR_DIAGNOSTICS_HPP_
