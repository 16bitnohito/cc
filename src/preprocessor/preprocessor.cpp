#include <preprocessor/preprocessor.hpp>

#include <array>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <preprocessor/utility.hpp>

using namespace std;


namespace pp {
namespace {

std::vector<std::tuple<std::string, Token::Type>> directives = {
	{ "include", Token::kInclude },
	{ "define", Token::kDefine },
	{ "undef", Token::kUndef },
	{ "if", Token::kIf },
	{ "ifdef", Token::kIfdef },
	{ "ifndef", Token::kIfndef },
	{ "elif", Token::kElif },
	{ "else", Token::kElse },
	{ "endif", Token::kEndif },
	{ "error", Token::kError },
	{ "line", Token::kLine },
	{ "pragma", Token::kPragma },
};
Sorter directives_sorter(directives, [](const auto& lhs, const auto& rhs) {
	return get<0>(lhs) < get<0>(rhs);
});

Token::Type as_directive(const Token& token) {
	const string& s = token.string();
	auto it = lower_bound(begin(directives), end(directives), s,
			[](const auto& lhs, const auto& rhs) {
				return get<0>(lhs) < rhs;
			});
	if (it != directives.end() && !(s < get<0>(*it))) {
		return get<1>(*it);
	} else {
		return token.type();
	}
}

const char* kLevelTag[] = {
	u8"",
	u8"致命的エラー",
	u8"エラー",
	u8"警告",
	u8"情報",
	u8"トレース",
	u8"デバッグ",
};

#if HOST_PLATFORM == PLATFORM_WINDOWS
const std::string kPathDelimiter = u8"\\";
#else
const std::string kPathDelimiter = u8"/";
#endif

std::string escape_string(const std::string& s) {
	string result;
	result.reserve(s.length());

	for (auto c : s) {
		if (c == '"') {
			result += "\\\"";
		} else if (c == '\\') {
			result += "\\\\";
		} else {
			result += c;
		}
	}

	return result;
}

std::string quote_string(const std::string& s) {
	string result;
	result.reserve(s.length() + 2);

	result += '"';
	result += escape_string(s);
	result += '"';

	return result;
}

std::string unquote_string(const std::string& s) {
	string result;
	result.reserve(s.length());

	for (size_t i = 1; i < (s.length() - 1); i++) {
		char c = s[i];
		if (c == '\\') {
			char c2 = s[i + 1];
			if (c2 == '"') {
				c = '"';
				i++;
			} else if (c2 == '\\') {
				c = '\\';
				i++;
			}
		}
		result += c;
	}

	return result;
}

std::string destringize(const std::string& s) {
	string result = s;

	if (result.find("u8\"") == 0) {
		result.erase(0, 2);
	} else if (result.find("u\"") == 0) {
		result.erase(0, 1);
	} else if (result.find("U\"") == 0) {
		result.erase(0, 1);
	} else if (result.find("L\"") == 0) {
		result.erase(0, 1);
	}

	return unquote_string(result);
}

inline
Token shrink_ws(const Token& t) {
	assert(t.type() == Token::kWhiteSpace || t.type() == Token::kComment);
	return Token(Token::kTokenValueASpace, t.line(), t.column());
}

TokenList shrink_ws_tokens(const TokenList& ts) {
	TokenList result;
	result.reserve(ts.size());

	for (const auto& t : ts) {
		if (t.type() == Token::kNewLine) {
			// skip
		} else if (t.type() == Token::kComment || t.type() == Token::kWhiteSpace) {
			if (!result.empty() && !result.back().is_ws()) {
				result.push_back(shrink_ws(t));
			}
		} else {
			result.push_back(t);
		}
	}
	if (!result.empty() && result.back().is_ws()) {
		result.pop_back();
	}

	return result;
}

const Operator kOperatorOpenParen = { kOpOpenParen, "(", 254, 0 };
const Operator kOperatorCloseParen = { kOpCloseParen, ")", 255, 0 };
const Operator kOperatorUnknown = { kOpUnknown, "", 0, 0 };
const Operator kOperatorPlus = { kOpPlus, "+", 2, 1 };
const Operator kOperatorMinus = { kOpMinus, "-", 2, 1 };

std::vector<Operator> operators = {
	kOperatorUnknown,
	kOperatorCloseParen,
	kOperatorOpenParen,
	//{ kOpComma, ",", 15, 2 },
	{ kOpColon, ":", 13, 3 },
	{ kOpCond, "?", 13, 1 },
	{ kOpOr, "||", 12, 2 },
	{ kOpAnd, "&&", 11, 2 },
	{ kOpBitOr, "|", 10, 2 },
	{ kOpBitXor, "^", 9, 2 },
	{ kOpBitAnd, "&", 8, 2 },
	{ kOpEq, "==", 7, 2 },
	{ kOpNeq, "!=", 7, 2 },
	{ kOpLt, "<", 6, 2 },
	{ kOpGt, ">", 6, 2 },
	{ kOpLeq, "<=", 6, 2 },
	{ kOpGeq, ">=", 6, 2 },
	{ kOpShl, "<<", 5, 2 },
	{ kOpShr, ">>", 5, 2 },
	{ kOpAdd, "+", 4, 2 },
	{ kOpSub, "-", 4, 2 },
	{ kOpMul, "*", 3, 2 },
	{ kOpDiv, "/", 3, 2 },
	{ kOpMod, "%", 3, 2 },
	//{ kOpPlus, "+", 2, 1 },
	//{ kOpMinus, "-", 2, 1 },
	{ kOpNot, "!", 2, 1 },
	{ kOpCompl, "~", 2, 1 },
};
Sorter operators_sorter(operators,
		[](const auto& lhs, const auto& rhs) { return lhs.sequence < rhs.sequence; });

const Operator& string_to_op(const std::string& s) {
	auto it = lower_bound(operators.begin(), operators.end(), s,
		[](const auto& lhs, const auto& rhs) { return lhs.sequence < rhs; });
	if (it != operators.end() && !(s < it->sequence)) {
		return *it;
	} else {
		return kOperatorUnknown;
	}
}

}	//  anonymous namespace


const Token kTokenNull("", Token::kNull);
const Token kTokenEndOfFile("", Token::kEndOfFile);
const Token kTokenASpace(" ", Token::kWhiteSpace);
const Token kTokenOpenParen("(", Token::kPunctuator);
const Token kTokenCloseParen(")", Token::kPunctuator);
const Token kTokenComma(",", Token::kPunctuator);
const Token kTokenEllipsis("...", Token::kPunctuator);
const Token kTokenOpStringize("#", Token::kPunctuator);
const Token kTokenOpConcat("##", Token::kPunctuator);
const Token kTokenOpDefined("defined", Token::kIdentifier);
const Token kTokenOpPragma("_Pragma", Token::kIdentifier);
const Token kTokenStdc("STDC", Token::kIdentifier);
const Token kTokenVaArgs("__VA_ARGS__", Token::kIdentifier);
const Token kTokenVaOpt("__VA_OPT__", Token::kIdentifier);


Macro::Macro(const std::string& name, const TokenList& replist, const std::string& source, const Token& name_token) {
	name_ = name;
	is_predefined_ = false;
	reset(replist, source, name_token);
}

Macro::Macro(const std::string& name, const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token) {
	name_ = name;
	is_predefined_ = false;
	reset(params, replist, source, name_token);
}

Macro::Macro(const std::string& name, const std::string& value, const Token::Type type) {
	name_ = name;
	is_predefined_ = true;
	reset({ Token(value, type) }, "", kTokenNull);
}

Macro::~Macro() {
}

size_t Macro::param_index_of(const std::string& param_name) const {
	size_t i = 0;
	for (auto& p : params()) {
		if (p == param_name) {
			break;
		}
		i++;
	}
	return i;
}

void Macro::reset(const TokenList& replist, const std::string& source, const Token& name_token) {
	form_ = kObjectLike;
	expantion_method_ = get_expantion_method(kObjectLike, name_, replist);
	params_ = Macro::kNoParams;
	replist_ = replist;
	has_va_args_ = false;
	source_ = source;
	line_ = name_token.line();
	column_ = name_token.column();
}

void Macro::reset(const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token) {
	form_ = kFunctionLike;
	expantion_method_ = get_expantion_method(kFunctionLike, name_, replist);
	params_ = params;
	replist_ = replist;
	has_va_args_ = (params.empty()) ? false : (params.back() == kTokenEllipsis.string());
	source_ = source;
	line_ = name_token.line();
	column_ = name_token.column();
}

Macro::ExpantionMethod Macro::get_expantion_method(Form /*form*/, const std::string& name, const TokenList& replist) {
	if (name == kTokenOpPragma.string()) {
		return kExpantionMethodOpPragma;
	} else {
		bool directly = true;
		for (const auto& t : replist) {
			if (t.type() == Token::kIdentifier ||
				t == kTokenOpStringize ||
				t == kTokenOpConcat) {
				directly = false;
				break;
			}
		}
		return directly ? kExpantionMethodDirectlyCopyable : kExpantionMethodNormal;
	}
}


Preprocessor::Options::Options() {
	input_encoding_ = "utf-8";	//  今は気分的なもの。
	//input_filepath_;
	output_encoding_ = "utf-8";	//  今は気分的なもの。
	//output_filepath_;
	output_line_directive_ = true;
	output_comment_ = false;
	support_trigraphs_ = false;
	//additional_include_dirs_;
}

Preprocessor::Options::~Options() {
}

const std::string& Preprocessor::Options::input_filepath() const {
	return input_filepath_;
}

const std::string& Preprocessor::Options::output_filepath() const {
	return output_filepath_;
}

const std::string& Preprocessor::Options::error_log_filepath() const {
	return error_log_filepath_;
}

bool Preprocessor::Options::output_line_directive() const {
	return output_line_directive_;
}

bool Preprocessor::Options::output_comment() const {
	return output_comment_;
}

bool Preprocessor::Options::support_trigraphs() const {
	return support_trigraphs_;
}

const std::vector<std::string>& Preprocessor::Options::additional_include_dirs() const {
	return additional_include_dirs_;
}

const std::vector<std::string>& Preprocessor::Options::macro_defs() const {
	return macro_defs_;
}

const std::vector<std::string>& Preprocessor::Options::macro_undefs() const {
	return macro_undefs_;
}

bool Preprocessor::Options::parse_options(const std::vector<std::string>& args) {
	int argc = static_cast<int>(args.size());

	for (int i = 1; i < argc; i++) {
		const string& arg = args[i];

		if (arg[0] == '-') {
			switch (arg[1]) {
			case '\0':
				if (input_filepath_.empty()) {
					input_filepath_ = arg;
				}
				break;
			case 'I': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				additional_include_dirs_.push_back(path);
				break;
			}
			case 'D': {
				string macro;
				if (arg[2] != '\0') {
					macro = &arg[2];
				} else {
					if ((i + 1) < argc) {
						macro = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				macro_defs_.push_back(macro);
				break;
			}
			case 'U': {
				string name;
				if (arg[2] != '\0') {
					name = &arg[2];
				} else {
					if ((i + 1) < argc) {
						name = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				macro_undefs_.push_back(name);
				break;
			}
			//  TODO: 将来的には。
			//case 'P': {
			//	output_line_directive_ = false;
			//	break;
			//}
			//case 'C': {
			//	output_comment_ = true;
			//	break;
			//}
			case 'o': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				if (output_filepath_.empty()) {
					output_filepath_ = path;
				}
				break;
			}
			case 'e': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				error_log_filepath_ = path;
				break;
			}
			case 't':
				if (arg == "-trigraphs") {
					support_trigraphs_ = true;
				}
				break;
			case 'h':
				return false;
				break;

			default:
				fprintf(stderr, kUnknownOptionError, arg[1]);
				return false;
			}
		} else {
			if (input_filepath_.empty()) {
				input_filepath_ = arg;
			}
		}
	}

	return true;
}

void Preprocessor::Options::print_usage() {
	printf(u8"使い方: cpp [options] input\n");
	printf(u8"Options:\n");
	printf(u8"-D <name>[=definition]\tマクロを定義する。\n");
	printf(u8"-D <name(params)>[=definitiion]\tマクロを定義する。\n");
	printf(u8"-U <name>\tマクロを削除する。\n");
	printf(u8"-I <directory>\t\tインクルード検索パスを追加する。\n");
	printf(u8"-o <file>\t出力先を指定する。(デフォルトは標準出力)\n");
	printf(u8"-e <file>\tエラー出力先を指定する。(デフォルトは標準エラー出力)\n");
	printf(u8"-h\t\tヘルプを出力する。\n");
}


Preprocessor::Source::Source(const std::string& filepath, Scanner* src_scanner, Preprocessor* preprocessor) {
	path = filepath;
	line = 1;
	pp = preprocessor;
	scanner = src_scanner;
	p = 0;
	lookahead.resize(kNumLookahead);
	condition_level = 0;
	//queue;
	q = 0;

	for (int i = 0; i < kNumLookahead; ++i) {
		consume();
	}
}

Preprocessor::Source::~Source() {
	assert(q == queue.size());
}

std::string Preprocessor::Source::parent_dir() {
	string::size_type i = path.rfind(kPathDelimiter);
	if (i == string::npos) {
		return "";
	} else {
		return path.substr(0, (i - 0));
	}
}

void Preprocessor::Source::reset_line_number(uint32_t new_line_number) {
	for (int i = 0; i < kNumLookahead; i++) {
		Token& t = lookahead[(p + i) % kNumLookahead];
		t.line(new_line_number);

		if (t.type() == Token::kNewLine) {
			new_line_number++;
		}
	}
	scanner->line_number(new_line_number);
	line = new_line_number;
}


Preprocessor::Preprocessor()
	: include_dirs_()
	, output_(stdout)
	, error_output_(stderr)
	, sources_()
	, macros_()
	, predef_macro_names_()
{
	diag_level_ = DiagLevel::kWarning;
	warning_count_ = 0;
	error_count_ = 0;
	included_files_ = 0;
	inclusion_depth_ = 0;

	clock_start_ = clock();
}

Preprocessor::~Preprocessor() {
	cleanup();
}

bool Preprocessor::has_error() {
	return error_count_ > 0;
}

bool Preprocessor::parse_options(const std::vector<std::string>& args) {
	return opts_.parse_options(args);
}

void Preprocessor::print_usage() {
	opts_.print_usage();
}

int Preprocessor::run() {
	//  TODO: コマンドラインで指定されたもの以外でインクルードパスを追加するならここで。

	const vector<string>& dirs = opts_.additional_include_dirs();
	include_dirs_.insert(include_dirs_.end(), dirs.begin(), dirs.end());

	// TODO: 入出力周りを整理して、iostreamにした方が良いかも。
	string err_path = opts_.error_log_filepath();
	if (err_path.empty()) {
		error_output_ = stderr;
	} else {
#if HOST_PLATFORM == PLATFORM_WINDOWS
		errno_t e = _wfopen_s(&error_output_, path_string(err_path).c_str(), L"w");
#else
		errno_t e = fopen_s(&error_output_, path_string(err_path).c_str(), "w");
#endif
		if (e != 0) {
			error_output_ = stderr;
			output_error(kNoSuchFileError, err_path.c_str());
			return 1;
		}
	}
	Diagnostics::setup(error_output_);

	//
	string out_path = opts_.output_filepath();
	if (out_path.empty()) {
		output_ = stdout;
	} else {
#if HOST_PLATFORM == PLATFORM_WINDOWS
		errno_t e = _wfopen_s(&output_, path_string(out_path).c_str(), L"w");
#else
		errno_t e = fopen_s(&output_, path_string(out_path).c_str(), "w");
#endif
		if (e != 0) {
			output_error(kNoSuchFileError, out_path.c_str());
			return 1;
		}
	}

	//
	string in_path = opts_.input_filepath();
	if (in_path.empty()) {
		output_error(kNoInputError);
		return 1;
	}

	istream* in = &cin;
	ifstream in_file;
	if (in_path != "-") {
		in_file.open(path_string(in_path));
		if (!in_file.is_open()) {
			output_error(kNoSuchFileError, in_path.c_str());
			return 1;
		}

		in = &in_file;
	}
	prepare_predefined_macro();
	preprocessing_file(in, in_path);

	//output_error("%d errors, %d warnings.\n", error_count_, warning_count_);

	if (!cleanup()) {
		return 1;
	}

	return has_error();
}

bool Preprocessor::cleanup() {
	bool e = false;

	if (output_ != nullptr) {
		fflush(output_);
		if (ferror(output_) != 0) {
			output_error(kFileOutputError);
			e = true;
		}

		clock_end_ = clock() - clock_start_;
		info(kTokenNull, "elapsed: %ld", clock_end_);

		fclose(output_);
		output_ = nullptr;
	}

	if (error_output_ != nullptr) {
		fflush(error_output_);
		fclose(error_output_);
		error_output_ = nullptr;
	}

	return !e;
}

void Preprocessor::prepare_predefined_macro() {
	time_t now;
	time(&now);

	static constexpr int kTmBaseYear = 1900;
	static constexpr char month[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	tm t;
	localtime_s(&t, &now);
	char date[14];
	char time[11];
	sprintf_s(date, sizeof(date), "%.3s %2d %4d", month[t.tm_mon], t.tm_mday, kTmBaseYear + t.tm_year);
	sprintf_s(time, sizeof(time), "%.2d:%.2d:%.2d", t.tm_hour, t.tm_min, t.tm_sec);

	//  定義済みマクロをここで追加する。
	predef_macro_names_.clear();

	add_predefined_macro("__DATE__",         quote_string(date), Token::kStringLiteral);
	add_predefined_macro("__FILE__",         quote_string(""),   Token::kStringLiteral);
	add_predefined_macro("__LINE__",         "0",                Token::kPpNumber);
	add_predefined_macro("__STDC__",         "1",                Token::kPpNumber);
	add_predefined_macro("__STDC_HOSTED__",  "0",                Token::kPpNumber);
	add_predefined_macro("__STDC_VERSION__", "201112L",          Token::kPpNumber);
	add_predefined_macro("__TIME__",         quote_string(time), Token::kStringLiteral);

	//
	//add_predefined_macro("__STDC_ISO_10646__",       "yyyymmL", Token::kPpNumber);
	//add_predefined_macro("__STDC_MB_MIGHT_NEQ_WC__", "1",       Token::kPpNumber);
	//add_predefined_macro("__STDC_UTF_16__",          "1",       Token::kPpNumber);
	//add_predefined_macro("__STDC_UTF_32__",          "1",       Token::kPpNumber);

	//
	//add_predefined_macro("__STDC_ANALYZABLE__",      "1",       Token::kPpNumber);
	//add_predefined_macro("__STDC_IEC_559__",         "1",       Token::kPpNumber);
	//add_predefined_macro("__STDC_IEC_559_COMPLEX__", "1",       Token::kPpNumber);
	//add_predefined_macro("__STDC_LIB_EXT1__",        "201109L", Token::kPpNumber);
	add_predefined_macro("__STDC_NO_ATOMICS__", "1", Token::kPpNumber);
	add_predefined_macro("__STDC_NO_COMPLEX__", "1", Token::kPpNumber);
	add_predefined_macro("__STDC_NO_THREADS__", "1", Token::kPpNumber);
	add_predefined_macro("__STDC_NO_VLA__",     "1", Token::kPpNumber);

	//  マクロではないが、マクロ定義の対象にならない？ようなのでここで追加する。
	predef_macro_names_.push_back(kTokenOpDefined.string());

	//  仕様的な定義済みマクロはここまでとして、後の検索の為にソートしておく。
	sort(predef_macro_names_.begin(), predef_macro_names_.end());

	//  _Pragma演算子はマクロとして扱う。definedと違って括弧が必要なので、所々で特別扱いしないで済む。
	//  実際の処理は独自のメソッドで行われるので置換リストは適当に。
	auto op_pragma = make_shared<Macro>(
			kTokenOpPragma.string(), Macro::ParamList{ "content" }, TokenList{ Token("content", Token::kIdentifier) }, "", kTokenNull);
	auto result = macros_.insert({ op_pragma->name(), op_pragma });
	if (!result.second) {
		fatal_error(kTokenNull, __func__ /* ロジックエラーかメモリーが足りないか？ */);
	}

	//  NOTE: 処理系定義のマクロが必要になった時には、ここで追加する。
	//        その際は、predef_macro_names_とは別にリストを用意して、warning扱い
	//        すると良いかも？

	//  オプション指定のマクロ定義はここから。定義済みマクロの後にしないと失敗する。
	auto& defs = opts_.macro_defs();
	for (auto def : defs) {
		auto i = def.find('=');
		if (i == string::npos) {
			//  定義が無いか、或いは、コマンドライン引数を引用符で囲って、#defineの様に
			//  空白で区切っているかもしれない。後者の場合、明確にサポートするものでは
			//  ないが、そのまま通している。
		} else {
			//  #defineでの処理を使えるようにするため、空白に置き換える。
			//  名前と置換リストが空白で区切られており、置換リストに '='が含まれる場合を
			//  考慮していない。
			def[i] = ' ';
		}

		//  トークンのマッチングの都合上、改行を加えてから定義を実行している。
		push_stream(def);
		execute_define();
		pop_stream();
	}

	//  事前に定義できるものを定義した上で、オプション指定の定義削除を実行する。
	//  そうでなければ、何を定義解除するのか不明である。
	auto& undefs = opts_.macro_undefs();
	for (const auto& name : undefs) {
		push_stream(name);
		execute_undef();
		pop_stream();
	}
}

void Preprocessor::preprocessing_file(std::istream* input, const std::string& path) {
	Scanner scanner(input, opts_.support_trigraphs());
	Source source(path, &scanner, this);
	Group root(true, source.line, Token::kNull);

	if constexpr (false) {
		Token t = scanner.next_token();
		while (t.type() != Token::kEndOfFile) {
			t = scanner.next_token();
			if (t.type() == Token::kNewLine) {
				output_text("[kNewLine]\n");
			} else {
				output_text(quote_string(t.string()) + "(" + Token::type_to_string(t.type()) + ")");
			}
		}
	} else {
		//group()?;
		sources_.push(&source);
		while (peek(1).type() != Token::kEndOfFile) {
			group(*sources_.top(), root);
		}
		sources_.pop();
	}
}

void Preprocessor::group(Source& source, Group& group) {
	//group_part();
	//group(); group_part();

	GroupScope scope(&source, &group);

	if (source.groups.size() >= kMinSpecConditionalInclusion) {
		info(peek(1), kMinSpecConditionalInclusionWarning, kMinSpecConditionalInclusion, source.groups.size());
	}

	do {
		if (!group_part()) {
			break;
		}
	} while (peek(1).type() != Token::kEndOfFile);
}

bool Preprocessor::group_part() {
	TokenList ws_tokens;
	skip_ws(&ws_tokens);

	if (peek(1).type() == Token::kPunctuator && peek(1).string() == "#") {
		match(Token::kPunctuator);
		skip_ws();

		Source& src = *sources_.top();
		Token dir_token = peek(1);
		Token::Type dir = as_directive(dir_token);
		Token::Type cur_group = src.groups.top()->type;

		if (((cur_group == Token::kIf)     && (dir == Token::kElif || dir == Token::kElse || dir == Token::kEndif)) ||
			((cur_group == Token::kIfdef)  && (dir == Token::kElif || dir == Token::kElse || dir == Token::kEndif)) ||
			((cur_group == Token::kIfndef) && (dir == Token::kElif || dir == Token::kElse || dir == Token::kEndif)) ||
			((cur_group == Token::kElif)   && (dir == Token::kElif || dir == Token::kElse || dir == Token::kEndif)) ||
			((cur_group == Token::kElse)   && (dir == Token::kEndif))) {
			return false;
		}

		if (dir == Token::kIf ||
			dir == Token::kIfdef ||
			dir == Token::kIfndef) {
			if_section();
		} else if (
			dir == Token::kInclude ||
			dir == Token::kDefine ||
			dir == Token::kUndef ||
			dir == Token::kLine ||
			dir == Token::kError ||
			dir == Token::kPragma ||
			dir == Token::kNewLine) {
			control_line(dir);
		} else {
			if (src.condition_level == 0) {
				if (dir == Token::kElif) {
					error(dir_token, kElifWithoutIfError);
				} else if (dir == Token::kElse) {
					error(dir_token, kElseWithoutIfError);
				} else if (dir == Token::kEndif) {
					error(dir_token, kEndifWithoutIfError);
				}
			} else {
				if (cur_group == Token::kElse && dir == Token::kElif) {
					error(dir_token, kElifAfterElseError);
				}
			}

			non_directive();
		}
	} else {
		text_line(ws_tokens);
	}
	return true;
}

void Preprocessor::if_section() {
	skip_ws();

	Source& src = *sources_.top();
	bool processed = false;

	src.condition_level++;

	Token if_token = peek(1);
	Token::Type dir = as_directive(if_token);
	if (dir == Token::kIf ||
		dir == Token::kIfdef ||
		dir == Token::kIfndef) {
		processed = if_group();
	} else {
		fatal_error(if_token, __func__ /* ロジックエラーっぽい */);
	}

	//  ここに来た時点で "#"は consume済み。
	skip_ws();

	if (as_directive(peek(1)) == Token::kElif) {
		//processed = elif_groups(processed);
		Token t;
		do {
			processed = elif_group(processed) || processed;
		} while (as_directive(peek(1)) == Token::kElif);
	}

	skip_ws();
	if (as_directive(peek(1)) == Token::kElse) {
		else_group(processed);
	}

	skip_ws();
	if (as_directive(peek(1)) == Token::kEndif) {
		endif_line();
	} else {
		error(if_token, kUnterminatedIfError, if_token.line());
	}

	src.condition_level--;
}

TokenList Preprocessor::make_constant_expression() {
	skip_ws();

	bool bad_expr = false;
	TokenList expr;
	Token t;
	while (!peek(1).is_eol() && !bad_expr) {
		t = peek(1);
		consume();

		if (t.type() != Token::kIdentifier) {
			expr.push_back(t);
			continue;
		}
		if (t == kTokenVaArgs) {
			error(t, kVaArgsIdentifierUsageError);
			bad_expr = true;
			continue;
		}

		if (t == kTokenOpDefined) {
			skip_ws();
			bool open = (peek(1) == kTokenOpenParen);
			if (open) {
				match(kTokenOpenParen.type());
				skip_ws();
			}

			Token macro_name = peek(1);
			consume();
			if (macro_name.type() != Token::kIdentifier) {
				error(peek(1), __func__ /* マクロ名が指定されていなかった？構文がおかしいので失敗 */);
				bad_expr = true;
			} else {
				MacroPtr m = find_macro(macro_name.string());
				Token v((m != nullptr) ? "1" : "0", Token::kPpNumber);
				expr.push_back(v);

				if (open) {
					skip_ws();
					if (peek(1) != kTokenCloseParen) {
						error(peek(1), __func__ /* 閉じ括弧が無い。構文がおかしいので失敗 */);
						bad_expr = true;
					}
					match(kTokenCloseParen.type());
				}
			}
		} else {
			const MacroPtr m = find_macro(t.string());
			if (m == nullptr) {
				expr.push_back(Token("0", Token::kPpNumber));
			} else {
				TokenList replaced;
				if (sources_.top()->path.find("check.hpp") != string::npos &&
					sources_.top()->line == 27) {
					replaced = replaced;
				}
				if (!m->is_function()) {
					DEBUG(t, "[START]: %s", m->name().c_str());
					expand(*m, Macro::kNoArgs, replaced);
				} else {
					skip_ws();

					if (peek(1) != kTokenOpenParen) {
						warning(t, kFuncTypeMacroUsageWarning);
						bad_expr = true;
					} else {
						auto args = read_macro_args(*m, nullptr);
						if (!args.has_value()) {
							bad_expr = true;
						} else {
							DEBUG(t, "[START]: %s", m->name().c_str());
							expand(*m, *args, replaced);
						}
					}
				}

				replace_stream(move(replaced));
			}
		}
	}
	//new_line();

	if (expr.empty() || bad_expr) {
		expr.push_back(Token("0", Token::kPpNumber));
	}

	return expr;
}

target_intmax_t Preprocessor::constant_expression(TokenList&& expr_tokens, const Token& dir_token) {
	//  TODO: make_constant_expressionの中身をここに移して、ここの中身は calculator.cppにしたい。

#if !defined(NDEBUG)
	string expr_string = Token::concat_string(expr_tokens);
#endif

	push_stream(expr_tokens);
	stack<Operator> ops;
	stack<target_intmax_t> nums;
	bool bad_expr = false;
	bool unary = true;

	ops.push(kOperatorOpenParen);

	skip_ws();
	Token t = peek(1);
	while (!t.is_eol() && !bad_expr) {
		switch (t.type()) {
		case Token::kPpNumber: {
			string s = t.string();
			match(Token::kPpNumber);

			target_intmax_t n = 0;
			if (!parse_int(s, &n)) {
				error(t, kConstantNumberIsNotAIntegerError, s.c_str());
				bad_expr = true;
			}
			nums.push(n);
			unary = false;
			break;
		}
		case Token::kCharacterConstant: {
			//  TODO: 全くまともに作っていない。kPpNumberの場合と同じようにするには辛いので、Scanner側で
			//      数値を求めておきたい。
			string s = t.string();
			match(Token::kCharacterConstant);

			auto i = s.find('\'');
			int c = s[i + 1];
			nums.push(c);
			unary = false;
			break;
		}
		case Token::kPunctuator: {
			string opstr = t.string();
			match(Token::kPunctuator);

			Operator op = string_to_op(opstr);

			if (unary) {
				switch (op.id) {
				case kOpAdd:
					op = kOperatorPlus;
					break;

				case kOpSub:
					op = kOperatorMinus;
					break;

				case kOpCompl:
				case kOpNot:
				case kOpOpenParen:
				case kOpCloseParen:
					//  差し替えは必要ない。
					break;

				case kOpUnknown:
					//  下の　kInvalidOperatorErrorにするのでここでは何もしない。
					break;

				default:
					error(t, kInvalidConstantExpressionError);
					bad_expr = true;
					break;
				}
			}
			if (op.id == kOpUnknown) {
				error(dir_token, kInvalidOperatorError, opstr.c_str());
				bad_expr = true;
			}

			if (!bad_expr) {
				if (op.id == kOpOpenParen) {
					ops.push(kOperatorOpenParen);
					unary = false;
				} else {
					if (!unary) {
						calc(ops, nums, op);
					}
					if (op.id == kOpCloseParen) {
						ops.pop();
						unary = false;
					} else {
						ops.push(op);
						unary = true;
					}
				}
			}
			break;
		}
		default:
			consume();
			error(t, kInvalidConstantExpressionError);
			bad_expr = true;
			break;
		}

		skip_ws();
		t = peek(1);
	}
	if (bad_expr) {
		pop_stream();
		return 0;
	}

	calc(ops, nums, kOperatorCloseParen);

	if (nums.size() != 1) {
		pop_stream();
		error(dir_token, __func__);
		return 0;
	}

	target_intmax_t result = nums.top();
	nums.pop();

	pop_stream();
	return result;
}

void Preprocessor::calc(stack<Operator>& ops, stack<target_intmax_t>& nums, const Operator& next_op) {
	if (ops.empty()) {
		return;
	}

	int nest = 0;
	while (!ops.empty() &&
		((next_op.id != kOpCloseParen && next_op.precedence > ops.top().precedence) ||
		(next_op.id == kOpCloseParen && !(nest == 0 && ops.top().id == kOpOpenParen)))) {
		if (ops.empty()) {
			break;
		}

		Operator op = ops.top();
		ops.pop();

		target_intmax_t result;
		switch (op.arity) {
		case 1: {
			if (nums.size() < 1) {
				break;
			}

			target_intmax_t l = nums.top();
			nums.pop();

			switch (op.id) {
			case kOpPlus:
				result = l;
				break;
			case kOpMinus:
				result = -l;
				break;
			case kOpCompl:
				result = ~l;
				break;
			case kOpNot:
				result = (l == 0) ? 1 : 0;
				break;
			case kOpCond:
				result = l;
				break;
			default:
				assert(false);
				break;
			}
			break;
		}
		case 2: {
			if (nums.size() < 2) {
				break;
			}

			target_intmax_t r = nums.top();
			nums.pop();
			target_intmax_t l = nums.top();
			nums.pop();

			switch (op.id) {
			case kOpOr:
				result = l || r;
				break;
			case kOpAnd:
				result = l && r;
				break;
			case kOpBitOr:
				result = l | r;
				break;
			case kOpBitXor:
				result = l ^ r;
				break;
			case kOpBitAnd:
				result = l & r;
				break;
			case kOpEq:
				result = l == r;
				break;
			case kOpNeq:
				result = l != r;
				break;
			case kOpLt:
				result = l < r;
				break;
			case kOpGt:
				result = l > r;
				break;
			case kOpLeq:
				result = l <= r;
				break;
			case kOpGeq:
				result = l >= r;
				break;
			case kOpShl:
				result = l << r;
				break;
			case kOpShr:
				result = l >> r;
				break;
			case kOpAdd:
				result = l + r;
				break;
			case kOpSub:
				result = l - r;
				break;
			case kOpMul:
				result = l * r;
				break;
			case kOpDiv:
				result = l / r;
				break;
			case kOpMod:
				result = l % r;
				break;
			//case kOpComma:
			//	result = r;
			//	break;
			default:
				assert(false);
				break;
			}
			break;
		}
		case 3: {
			if (nums.size() < 3) {
				break;
			}

			target_intmax_t r = nums.top();
			nums.pop();
			target_intmax_t l = nums.top();
			nums.pop();
			target_intmax_t c = nums.top();
			nums.pop();

			switch (op.id) {
			case kOpColon:
				result = c ? l : r;
				break;
			default:
				assert(false);
				break;
			}
			break;
		}
		default:
			switch (op.id) {
			case kOpUnknown:
				break;
			case kOpOpenParen:
				nest--;
				break;
			case kOpCloseParen:
				nest++;
				break;
			default:
				assert(false);
				break;
			}
		}

		if (op.arity > 0) {
			nums.push(result);
		}
	}
}

bool Preprocessor::if_group() {
	skip_ws();

	//"#" "if" constant_expression(); match(Token::kNewLine); group() ? ;
	//"#" "ifdef" match(Token::kIdentifier); match(Token::kNewLine); group() ? ;
	//"#" "ifndef" match(Token::kIdentifier); match(Token::kNewLine); group() ? ;
	target_intmax_t result;
	Source& src = *sources_.top();
	Token dir_token = peek(1);
	Token::Type dir = as_directive(dir_token);
	Group* parent = src.groups.top();

	if (!parent->processing) {
		skip_directive_line();
		new_line();
		result = 0;
	} else {
		if (dir == Token::kIf) {
			match("if");
			result = constant_expression(make_constant_expression(), dir_token);
		} else if (dir == Token::kIfdef) {
			match("ifdef");
			skip_ws();

			Token name_token = peek(1);
			if (name_token.type() != Token::kIdentifier) {
				skip_directive_line();
				result = 0;
			} else {
				string name = name_token.string();
				match(Token::kIdentifier);
				skip_ws();

				if (name_token == kTokenVaArgs) {
					error(name_token, kVaArgsIdentifierUsageError);
				}
				result = (find_macro(name) != nullptr);
			}
		} else if (dir == Token::kIfndef) {
			match("ifndef");
			skip_ws();

			Token name_token = peek(1);
			if (name_token.type() != Token::kIdentifier) {
				skip_directive_line();
				result = 0;
			} else {
				string name = name_token.string();
				match(Token::kIdentifier);
				skip_ws();

				if (name_token == kTokenVaArgs) {
					error(name_token, kVaArgsIdentifierUsageError);
				}
				result = (find_macro(name) == nullptr);
			}
		} else {
			fatal_error(dir_token, __func__ /* ロジックエラー */);
		}

		new_line();
	}

	Group g((result != 0) && parent->processing, src.line, dir);
	group(src, g);

	return g.processing;
}

bool Preprocessor::elif_groups(bool /*processed*/) {
	//elif_group();
	//elif_groups(); elif_group();

	//do {
	//	processed = elif_group(processed) || processed;
	//} while (peek(1).string() == "#" && as_directive(peek(2)) == Token::kElif);

	//return processed;
	assert(false && "elif_groupsは展開されている。");
	return false;
}

bool Preprocessor::elif_group(bool processed) {
	//"#" "elif" constant_expression(); new_line(); group() ? ;
	skip_ws();

	Token dir_token = peek(1);
	match("elif");
	skip_ws();

	Source& src = *sources_.top();
	Group* parent = src.groups.top();
	target_intmax_t result;
	if (!parent->processing || processed) {
		skip_directive_line();
		result = 0;
	} else {
		result = constant_expression(make_constant_expression(), dir_token);
	}
	new_line();

	Group g(parent->processing && !processed && (result != 0), src.line, Token::kElif);
	group(src, g);

	return g.processing;
}

void Preprocessor::else_group(bool processed) {
	//"#" "else" new_line(); group()?;
	skip_ws();
	match("else");
	skip_ws();
	new_line();

	Source& src = *sources_.top();
	Group* parent = src.groups.top();
	Group g(parent->processing && !processed, src.line, Token::kElse);
	group(src, g);
}

void Preprocessor::endif_line() {
	//"#" "endif" new_line();
	skip_ws();
	match("endif");
	skip_ws();
	new_line();
}

std::string replace_string(const std::string& s, const std::string& old_seq, const std::string& new_seq) {
	string result = s;
	string::size_type i = 0;

	while ((i = result.find(old_seq, i)) != string::npos) {
		result.replace(i, old_seq.length(), new_seq);
		i += old_seq.length();
	}

	return result;
}

//  TODO: std::filesystem、これ以外も。
std::string normalize_path(const std::string& path) {
	string result = path;
	string::size_type i;

#if HOST_PLATFORM == PLATFORM_WINDOWS
	result = replace_string(result, "/", kPathDelimiter);
#endif
	result = replace_string(result, kPathDelimiter + kPathDelimiter, kPathDelimiter);

	i = 0;
	do {
		const string cur = "." + kPathDelimiter;
		i = result.find(cur, i);
		if (i != string::npos) {
			if (i == 0 || result[i - 1] != '.') {
				result.replace(i, cur.length(), "");
			}
			i += 2;
		}
	} while (i != string::npos);

	return result;
}

void Preprocessor::control_line(Token::Type directive) {
	assert(directive == as_directive(peek(1)));

	//"#" "include" pp_tokens(Token::kHeaderName); new_line();
	//"#" "define" match(Token::kIdentifier); replacement_list(); new_line();

	//"#" "define" match(Token::kIdentifier); lparen();
	//while (? ? ? ) {
	//	identifier_list();
	//}
	//match(')');
	//replacement_list();
	//new_line();

	//"#" "define" match(Token::kIdentifier); lparen();
	//match("...");
	//match(')');
	//replacement_list();
	//new_line();

	//"#" "define" match(Token::kIdentifier); lparen();
	//identifier_list();
	//match(',');
	//match("...");
	//match(')');
	//replacement_list();
	//new_line();

	//"#" "undef" identifier(); new_line();
	//"#" "line" pp_tokens(); new_line();
	//"#" "error" pp_tokens() ? ; new_line();
	//"#" "pragma" pp_tokens() ? ; new_line();
	//"#" new_line();

	Source& src = *sources_.top();
	Group* cur_group = src.groups.top();

	if (!cur_group->processing) {
		skip_directive_line();
		new_line();

		return;
	}

	switch (directive) {
	case Token::kInclude: {
		src.scanner->state_hint(Scanner::kHintIncludeDirective);
		match("include");
		src.scanner->state_hint(Scanner::kHintInitial);
		skip_ws();

		Token header_name_token = peek(1);
		string header_name;
		if (header_name_token.type() == Token::kHeaderName) {
			header_name = header_name_token.string();
			match(Token::kHeaderName);
		} else if (header_name_token.type() == Token::kIdentifier) {
			match(Token::kIdentifier);

			TokenList expanded;
			const MacroPtr m = find_macro(header_name_token.string());
			if (m == nullptr) {
				error(header_name_token, __func__);
			} else {
				if (!m->is_function()) {
					expand(*m, Macro::kNoArgs, expanded);
				} else {
					skip_ws();
					if (peek(1) != kTokenOpenParen) {
						warning(header_name_token, kFuncTypeMacroUsageWarning);
					} else {
						auto args = read_macro_args(*m, nullptr);
						if (args.has_value()) {
							expand(*m, *args, expanded);
						}
					}
				}
			}
			if (expanded.empty()) {
				header_name = "";
			} else {
				header_name = Token::concat_string(expanded);
			}
		} else {
			skip_directive_line();
		}
		if (!peek(1).is_eol()) {
			header_name = "";
			skip_directive_line();
		}
		new_line();

		if (header_name.empty()) {
			error(header_name_token, kInvalidHeaderName);
		} else {
			execute_include(header_name, header_name_token);
		}
		break;
	}
	case Token::kDefine: {
		match("define");
		execute_define();
		break;
	}
	case Token::kUndef: {
		match("undef");
		execute_undef();
		break;
	}
	case Token::kLine: {
		match("line");
		skip_ws();

		bool has_num = false;
		bool has_path = false;

		Token num_token = peek(1);
		string numstr;
		if (num_token.type() == Token::kIdentifier) {
			match(Token::kIdentifier);

			TokenList expanded;
			const MacroPtr m = find_macro(num_token.string());
			if (m == nullptr) {
				error(num_token, __func__);
			} else {
				if (!m->is_function()) {
					expand(*m, Macro::kNoArgs, expanded);
				} else {
					skip_ws();
					if (peek(1) != kTokenOpenParen) {
						warning(num_token, kFuncTypeMacroUsageWarning);
					} else {
						auto args = read_macro_args(*m, nullptr);
						if (args.has_value()) {
							expand(*m, *args, expanded);
						}
					}
				}
			}
			if (expanded.empty()) {
				numstr = "";
			} else {
				numstr = Token::concat_string(expanded);
			}
		} else if (num_token.type() == Token::kPpNumber) {
			match(Token::kPpNumber);
			numstr = num_token.string();
		}

		int ln = 0;
		if (numstr.empty()) {
			error(num_token, __func__ /* マクロでも整数でもない */);
			skip_directive_line();
		} else {
			size_t next;
			ln = stoul(numstr, &next);
			if (next != numstr.length()) {
				error(num_token, __func__ /* 受け入れ可能な整数ではない */);
				skip_directive_line();
			}
			has_num = true;
		}

		skip_ws();
		string path;
		Token path_token = peek(1);
		if (path_token.is_eol()) {
			//  行番号のみの形式に合致したので、パスは無し。
		} else if (path_token.type() == Token::kStringLiteral) {
			//  行番号と名前の形式に合致した。
			path = path_token.string();
			consume();
		} else if (path_token.type() == Token::kIdentifier) {
			//  展開できれば名前になるかもしれないので、とりあえず展開してみる。
			match(Token::kIdentifier);

			TokenList expanded;
			const MacroPtr m = find_macro(path_token.string());
			if (m == nullptr) {
				error(path_token, __func__);
			} else {
				if (!m->is_function()) {
					expand(*m, Macro::kNoArgs, expanded);
				} else {
					skip_ws();
					if (peek(1) != kTokenOpenParen) {
						warning(path_token, kFuncTypeMacroUsageWarning);
					} else {
						auto args = read_macro_args(*m, nullptr);
						if (args.has_value()) {
							expand(*m, *args, expanded);
						}
					}
				}
			}
			path = Token::concat_string(expanded);
		}
		if (!path.empty() && (path[0] == '"' && path[path.length() - 1] == '"')) {
			has_path = true;
		}

		skip_ws();
		if (!peek(1).is_eol()) {
			error(peek(1), __func__ /* どの形式にも合致しないフォーマットエラー */);
			skip_directive_line();
			has_num = false;
			has_path = false;
		}
		new_line();

		if (has_num) {
			current_source_line_number(ln);
		}
		if (has_path) {
			current_source_path(unquote_string(path));
		}

		break;
	}
	case Token::kError: {
		Token t = peek(1);
		match("error");
		skip_ws();

		TokenList ts;
		skip_directive_line(&ts);
		ts = shrink_ws_tokens(ts);
		new_line();

		error(t, "#error " + Token::concat_string(ts));
		break;
	}
	case Token::kPragma: {
		match("pragma");
		skip_ws();

		Token first_token = peek(1);
		TokenList ts;
		skip_directive_line(&ts);
		new_line();

		ts = shrink_ws_tokens(ts);
		//if (first_token == kTokenStdc) {
		//}
		execute_pragma(ts, first_token);
		break;
	}
	case Token::kNewLine: {
		new_line();
		break;
	}
	default:
		fatal_error(peek(1), __func__ /* ロジックエラー */);
		break;
	}
}

void Preprocessor::text_line(const TokenList& ws_tokens) {
	//  pp_tokens()* new_line()

	Source& src = *sources_.top();
	Group* cur_group = src.groups.top();
	//bool output = cur_group->processing;

	//while (peek(1).type() != Token::kNewLine) {
	//	pp_tokens(output);
	//}
	//if (output) {
	//	output_text("\n");
	//}
	//new_line();

	if (!cur_group->processing) {
		while (!peek(1).is_eol()) {
			consume();
		}
		new_line();
		return;
	}

	for (const auto& ws : ws_tokens) {
		output_text(ws.string());
	}

	Token t;
	while (!peek(1).is_eol()) {
		t = peek(1);
		consume();

		if (t.type() == Token::kComment) {
			continue;
		}
		if (t.type() != Token::kIdentifier) {
			output_text(t.string());
			continue;
		}
		if (t == kTokenVaArgs) {
			error(t, kVaArgsIdentifierUsageError);
			output_text(t.string());
			continue;
		}
		if (t == kTokenVaOpt) {
			error(t, kVaOptIdentifierUsageError);
			output_text(t.string());
			continue;
		}
		const MacroPtr m = find_macro(t.string());
		if (m == nullptr) {
			output_text(t.string());
			continue;
		}

		bool dont_replace = false;
		bool dont_rescan = false;
		TokenList expanded;
		if (!m->is_function()) {
			DEBUG(t, "[START]: %s", m->name().c_str());
			dont_rescan = expand(*m, Macro::kNoArgs, expanded);
		} else {
			bool broken = false;
			TokenList ws;
			skip_ws_and_nl(&ws, &broken);

			if (peek(1) != kTokenOpenParen) {
				warning(t, kFuncTypeMacroUsageWarning);
				expanded = move(ws);
				dont_replace = true;

				if (broken && (peek(1).type() == Token::kPunctuator && peek(1).string() == "#")) {
					//  改行してディレクティブ行に到達したかもしれない。
					break;
				}
			} else {
				TokenList read_tokens;
				auto args = read_macro_args(*m, &read_tokens);
				if (!args.has_value()) {
					expanded.reserve(ws.size() + read_tokens.size());
					move(ws.begin(), ws.end(), back_inserter(expanded));
					move(read_tokens.begin(), read_tokens.end(), back_inserter(expanded));
					dont_replace = true;
				} else {
					DEBUG(t, "[START]: %s", m->name().c_str());
					dont_rescan = expand(*m, *args, expanded);
				}
			}
		}

		if (!dont_replace) {
			if (!dont_rescan) {
				replace_stream(move(expanded));
			} else {
				for (const auto& t2 : expanded) {
					output_text(t2.string());
				}
			}
		} else {
			output_text(t.string());
			for (const auto& t2 : expanded) {
				output_text(t2.string());
			}
		}
	}
	new_line();
	output_text("\n");
}

const TokenList& Preprocessor::get_expanded_arg(size_t n, const TokenList& arg, Macro::ArgList& cache) {
	if (!arg.empty() && cache[n].empty()) {
		push_stream(arg);
		scan(cache[n]);
		pop_stream();

		DEBUG(kTokenNull, "%s%d: %s --> %s", Indent::tab().c_str(), n, Token::concat_string(arg).c_str(), Token::concat_string(cache[n]).c_str());
	}

	return cache[n];
}

//bool Preprocessor::expand(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded) {
//}

bool Preprocessor::expand_directly_copyable(const Macro& macro, const Macro::ArgList& /*macro_args*/, TokenList& result_expanded) {
	result_expanded = macro.replist();
	return true;
}

bool Preprocessor::expand_normal(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded) {
	//  パラメーターのトークンを展開済み引数に置き換える。
	//  引数は必要時に展開される。
	Macro::ArgList expanded_args(macro_args.size());
	TokenList substituted;
	const TokenList& list = macro.replist();
	for (auto it = list.begin(); it != list.end(); ++it) {
		const Token& t = *it;

		if (t == kTokenOpStringize && macro.is_function()) {
			substituted.push_back(t);
			substituted.push_back(*++it);
		} else if (t == kTokenOpConcat) {
			substituted.push_back(t);
			substituted.push_back(*++it);
		} else {
			auto next_it = next(it);
			if (next_it != macro.replist().end() && *next_it == kTokenOpConcat) {
				substituted.push_back(t);
			} else {
				if (t.type() != Token::kIdentifier) {
					substituted.push_back(t);
				} else {
					if (macro.has_va_args() && t == kTokenVaArgs) {
						const auto i0 = macro.params().size() - 1;
						const auto end = macro_args.size();
						if (i0 < end) {
							const auto& a0 = get_expanded_arg(i0, macro_args[i0], expanded_args);
							substituted.insert(substituted.end(), a0.begin(), a0.end());

							for (auto i = macro.params().size(); i < end; ++i) {
								assert(false && "可変引数は 1つにまとめるようにしたので、このループには入らない。");
								substituted.push_back(kTokenComma);
								substituted.push_back(kTokenASpace);

								const auto& a = get_expanded_arg(i, macro_args[i], expanded_args);
								substituted.insert(substituted.end(), a.begin(), a.end());
							}
						}
					//  TODO: 実装してみたい。
					//} else if (macro.has_va_args() && t == kTokenVaOpt) {
						//  マクロ解析時に、
						//  __VA_OPT__の次が括弧であるかどうか、括弧が閉じているか、を確認しておき、失敗すれば
						//  マクロ定義を失敗(エラーを出力)させる。
						//
						//  その上で、ここでの置換は、
						//  if (i0 < end) { 括弧内のトークンを substituted.insertする。}
						//  else { 何もしない。 }
						//  という感じに。
					} else {
						size_t i = macro.param_index_of(t.string());
						if (i == macro.params().size()) {
							substituted.push_back(t);
						} else {
							const auto& a = get_expanded_arg(i, macro_args[i], expanded_args);
							substituted.insert(substituted.end(), a.begin(), a.end());
						}
					}
				}
			}
		}
	}

	//  文字列化(#)を行う。
	if (macro.is_function()) {
		for (auto it = substituted.begin(); it != substituted.end(); ++it) {
			if (*it == kTokenOpStringize) {
				auto next_it = next(it);
				if (!(next_it != substituted.end() &&
					next_it->type() == Token::kIdentifier &&
					(find(macro.params().begin(), macro.params().end(), next_it->string()) != macro.params().end() ||
						*next_it == kTokenVaArgs))) {
					fatal_error(*next_it, __func__ /* マクロ定義時に失敗しているはず */);
				}

				string s;
				if (macro.has_va_args() && *next_it == kTokenVaArgs) {
					s = execute_stringize(macro_args, macro.params().size() - 1, macro_args.size());
				} else {
					auto i = macro.param_index_of(next_it->string());
					if (i == macro.params().size()) {
						fatal_error(*next_it, __func__ /* マクロ定義時に失敗しているはず */);
					}
					s = execute_stringize(macro_args, i, i + 1);
				}

				it = substituted.erase(it, next(next_it));
				it = substituted.insert(it, Token(s, Token::kStringLiteral));
			}
		}
	}

	//  連結(##)を行う。
	for (auto it = substituted.begin(); it != substituted.end(); ) {
		if (*it != kTokenOpConcat) {
			++it;
		} else {
			decltype(it) prev_it;
			TokenList left;
			if (it == substituted.begin()) {
				prev_it = it;
			} else {
				prev_it = prev(it);
				left = substitute_by_arg_if_need(macro, macro_args, *prev_it);
			}

			auto next_it = next(it);
			TokenList right = substitute_by_arg_if_need(macro, macro_args, *next_it);

			//  連結結果を再トークン化する。
			Token l;
			Token r;
			if (left.empty()) {
				r = right.empty() ? kTokenNull : right.front();
			} else if (right.empty()) {
				l = left.back();
			} else {
				l = left.back();
				r = right.front();
			}

			//
			TokenList concat_result;
			concat_result.reserve(left.size() + right.size());
			if (!left.empty()) {
				copy(left.begin(), prev(left.end()), back_inserter(concat_result));
			}

			//
			istringstream in(l.string() + r.string());
			Scanner scanner(&in, opts_.support_trigraphs(), false);
			Token t = scanner.next_token();
			int count = 0;
			while (!t.is_eol()) {
				concat_result.push_back(t);
				t = scanner.next_token();
				++count;
			}

			if (count == 0) {
				//  空であれば何もしない。
			} else if (count == 1) {
				//  TODO: 事前に細かく分けて結合すべきなかもしれない。有り得るのは id+id=id, id+num=id,
				//        num+num=num, punct+punct=punctの 4パターン。punct同士以外については、ここに来たと
				//        しても、それは結合できたということなので、特に問題は無いだろうか。ただ、numは
				//        あくまでも pp-numberなのでパーサーがエラーにする可能性は残る。
				Token t2 = concat_result.front();
				if (t2 == kTokenOpConcat || t2.is_ws()) {
					error(r, kGeneratedInvalidPpTokenError2, l.string().c_str(), r.string().c_str());
					concat_result.pop_back();
					concat_result.push_back(Token(t2.string(), Token::kNonReplacementTarget));
				}
			} else {
				error(r, kGeneratedInvalidPpTokenError2, l.string().c_str(), r.string().c_str());
			}

			//
			if (!right.empty()) {
				copy(next(right.begin()), right.end(), back_inserter(concat_result));
			}

			//
			it = substituted.erase(prev_it, next(next_it));
			it = substituted.insert(it, concat_result.begin(), concat_result.end());
		}
	}

	DEBUG(kTokenNull, "%s--> %s", Indent::tab().c_str(), Token::concat_string(substituted).c_str());

	if (substituted.empty()) {
		result_expanded.clear();
	} else {
		rescan_count_++;
		used_macro_names_.insert(macro.name());
		push_stream(substituted);

		//  rescan
		scan(result_expanded);

		pop_stream();
		used_macro_names_.erase(macro.name());
		rescan_count_--;
	}

	return false;
}

bool Preprocessor::expand_op_pragma(const Macro& /*macro*/, const Macro::ArgList& macro_args, TokenList& /*result_expanded*/) {
	Macro::ArgList expanded_args(macro_args.size());

	if (macro_args.empty() || macro_args[0].empty()) {
		error(kTokenNull, kOpPragmaParameterTypeMismatch);
		return true;
	}
	get_expanded_arg(0, macro_args[0], expanded_args);

	if (expanded_args[0].size() != 1 ||
			expanded_args[0][0].type() != Token::kStringLiteral) {
		error(macro_args[0][0], kOpPragmaParameterTypeMismatch);
		return true;
	}

	Token pragma_text = expanded_args[0][0];
	string pragma = destringize(pragma_text.string());

	istringstream in(pragma);
	Scanner scanner(&in, opts_.support_trigraphs(), false);
	TokenList pragma_tokens;
	Token t = scanner.next_token();
	while (!t.is_eol()) {
		pragma_tokens.push_back(t);
		t = scanner.next_token();
	}
	pragma_tokens = shrink_ws_tokens(pragma_tokens);

	execute_pragma(pragma_tokens, macro_args[0][0]);

	return true;
}

TokenList Preprocessor::substitute_by_arg_if_need(const Macro& macro, const Macro::ArgList& macro_args, const Token& token) {
	if (token.type() != Token::kIdentifier) {
		//result.push_back(token);
		return { token };
	}

	TokenList result;
	if (macro.has_va_args() && token == kTokenVaArgs) {
		auto i0 = macro.params().size() - 1;
		const auto end = macro_args.size();
		if (i0 < end) {
			const TokenList& a0 = macro_args[i0];
			//if (a0.empty()) {
			//	result.push_back(kTokenPlaceMarker);
			//} else {
			result.insert(result.end(), a0.begin(), a0.end());
			//}
			for (auto i = macro.params().size(); i < end; ++i) {
				result.push_back(kTokenComma);
				result.push_back(kTokenASpace);

				const TokenList& a = macro_args[i];
				//if (a.empty()) {
				//	result.push_back(kTokenPlaceMarker);
				//} else {
				result.insert(result.end(), a.begin(), a.end());
				//}
			}
		}
	} else {
		auto i = macro.param_index_of(token.string());
		if (i == macro.params().size()) {
			result.push_back(token);
		} else {
			const TokenList& a = macro_args[i];
			//if (a.empty()) {
			//	result.push_back(kTokenPlaceMarker);
			//} else {
			result.insert(result.end(), a.begin(), a.end());
			//}
		}
	}

	return result;
}

void Preprocessor::scan(TokenList& result_expanded) {
	Token t;

	while (!peek(1).is_eol()) {
		t = peek(1);
		consume();

		if (t.type() != Token::kIdentifier) {
			result_expanded.push_back(t);
			continue;
		}
		if (t == kTokenVaArgs) {
			result_expanded.push_back(t);
			continue;
		}
		if (t == kTokenVaOpt) {
			result_expanded.push_back(t);
			continue;
		}
		if (used_macro_names_.find(t.string()) != used_macro_names_.end()) {
			DEBUG(t, "%s[USED]: %s", Indent::tab().c_str(), t.string().c_str());
			result_expanded.push_back(Token(t.string(), Token::kNonReplacementTarget));
			continue;
		}
		const MacroPtr m = find_macro(t.string());
		if (m == nullptr) {
			result_expanded.push_back(t);
			continue;
		}

		bool dont_replace = false;
		bool dont_rescan = false;
		TokenList expanded;
		if (!m->is_function()) {
			dont_rescan = expand(*m, Macro::kNoArgs, expanded);
		} else {
			TokenList ws;
			skip_ws(&ws);

			if (peek(1) != kTokenOpenParen) {
				expanded = move(ws);
				dont_replace = true;
			} else {
				TokenList read_tokens;
				auto args = read_macro_args(*m, &read_tokens);
				if (!args.has_value()) {
					expanded.reserve(ws.size() + read_tokens.size());
					move(ws.begin(), ws.end(), back_inserter(expanded));
					move(read_tokens.begin(), read_tokens.end(), back_inserter(expanded));
					dont_replace = true;
				} else {
					dont_rescan = expand(*m, *args, expanded);
				}
			}
		}

		if (!dont_replace) {
			if (!dont_rescan) {
				replace_stream(move(expanded));
			} else {
				result_expanded.reserve(result_expanded.size() + expanded.size());
				move(expanded.begin(), expanded.end(), back_inserter(result_expanded));
			}
		} else {
			result_expanded.reserve(result_expanded.size() + 1 + expanded.size());
			result_expanded.push_back(t);
			move(expanded.begin(), expanded.end(), back_inserter(result_expanded));
		}
	}
}

void Preprocessor::non_directive() {
	// pp_tokens new_line

	do {
		consume();
	} while (!peek(1).is_eol());
	new_line();
}

//void Preprocessor::lparen() {
//}

std::optional<TokenList> Preprocessor::replacement_list(
		Macro::Form macro_form, const Macro::ParamList& macro_params) {
	TokenList result;
	Token t = peek(1);

	while (!t.is_eol()) {
		if (t.is_ws()) {
			//  空白類が連続しないようにする。
			//  また、この条件により、置換リストの先頭に空白類が含まれない。
			if (!result.empty() && !result.back().is_ws()) {
				result.push_back(shrink_ws(t));
			}
		} else {
			result.push_back(t);
		}

		consume();
		t = peek(1);
	}
	//  末尾の空白を除く。
	if (!result.empty() && result.back().is_ws()) {
		result.pop_back();
	}

	//
	int old_error_count = error_count_;
	if (macro_form == Macro::kFunctionLike) {
		for (auto it = result.begin(); it != result.end(); it++) {
			if (*it == kTokenOpStringize) {
				//  #の次の空白は取り除いておく。
				auto ident = next(it);
				if (ident == result.end()) {
					error(*it, kOpStringizeNeedsParameterError);
					break;
				}

				if (ident->is_ws()) {
					ident = result.erase(ident);
					it = ident;
				}

				bool found_param = find(macro_params.begin(), macro_params.end(), ident->string()) != macro_params.end();
				if (!found_param && *ident != kTokenVaArgs) {
					error(*ident, kOpStringizeNeedsParameterError);
					break;
				}
			}
		}
	}

	for (auto it = result.begin(); it != result.end(); it++) {
		if (*it == kTokenOpConcat) {
			if (it == result.begin() || next(it) == result.end()) {
				error(*it, kOpConcatNeedsParameterError);
				break;
			}

			//  ##の前後の空白は取り除いておく。
			auto prev_it = prev(it);
			if (prev_it != result.begin() && prev_it->is_ws()) {
				it = result.erase(prev_it);
			}
			auto next_it = next(it);
			if (next_it != result.end() && next_it->is_ws()) {
				it = result.erase(next_it);
			}
		}
	}

	//  __VA_ARGS__を使えない場合にそれが見つかればエラーとする。
	if ((macro_form == Macro::kFunctionLike && (macro_params.empty() || (!macro_params.empty() && macro_params.back() != kTokenEllipsis.string()))) ||
			macro_form == Macro::kObjectLike) {
		Token va_args;
		auto it = find_if(result.begin(), result.end(),
				[&va_args](const auto& t) {
					if (t == kTokenVaArgs) {
						va_args = t;
						return true;
					} else {
						return false;
					}
				});
		if (it != result.end()) {
			error(va_args, kVaArgsIdentifierUsageError);
		}
	}

	if (error_count_ > old_error_count) {
		return nullopt;
	}

	return result;
}

std::optional<Macro::ParamList> Preprocessor::read_macro_params() {
	if (peek(1) != kTokenOpenParen) {
		fatal_error(peek(1), __func__ /* ロジック間違い */);
		//return nullopt;
	}

	match("(");
	skip_ws();

	bool bad_params = false;
	vector<string> params;
	set<string> added;
	while (peek(1) != kTokenCloseParen) {
		skip_ws();
		Token id = peek(1);
		consume();

		skip_ws();
		Token delim_or_close = peek(1);
		if (delim_or_close.type() != Token::kPunctuator ||
				(delim_or_close != kTokenComma && delim_or_close != kTokenCloseParen)) {
			error(id, kBadMacroParameterFormError);
			bad_params = true;
			break;
		} else {
			if (delim_or_close == kTokenComma) {
				match(",");
			}
		}

		if (id.type() == Token::kIdentifier) {
			if (id == kTokenVaArgs) {
				error(id, kVaArgsIdentifierUsageError);
				bad_params = true;
				break;
			}
			auto result = added.insert(id.string());
			if (!result.second) {
				error(id, kSameMacroParameterIdError);
				bad_params = true;
				break;
			}
			params.push_back(id.string());
		} else if (id == kTokenEllipsis) {
			params.push_back(id.string());
			if (delim_or_close != kTokenCloseParen) {
				error(id, kBadElipsisError);
				bad_params = true;
			}
			break;
		} else {
			error(id, kBadMacroParameterFormError);
			bad_params = true;
			break;
		}
	}
	if (bad_params) {
		return nullopt;
	}

	skip_ws();
	match(")");
	return params;
}

std::optional<Macro::ArgList> Preprocessor::read_macro_args(const Macro& macro, TokenList* read_tokens) {
	Token open_paren = peek(1);
	if (open_paren != kTokenOpenParen) {
		fatal_error(peek(1), __func__ /* ロジック間違い */);
		//return nullopt;
	}

	if (read_tokens) {
		read_tokens->push_back(open_paren);
	}
	match("(");

	Macro::ArgList args;
	args.reserve(macro.params().size());
	int comma = 0;

	Token t = peek(1);
	while (t != kTokenCloseParen) {
		int nest = 0;
		TokenList arg;

		if (!macro.has_va_args() ||
				(macro.has_va_args() && args.size() < macro.params().size() - 1)) {
			//  
			while (nest > 0 || (t != kTokenComma && t != kTokenCloseParen)) {
				if (t.type() == Token::kEndOfFile) {
					if (stream_stack_.empty()) {
						error(t, kBadMacroArgumentError);
					}
					return nullopt;
				}

				if (t == kTokenOpenParen) {
					nest++;
				} else if (t == kTokenCloseParen) {
					nest--;
				}

				if (read_tokens) {
					read_tokens->push_back(t);
				}
				arg.push_back(t);

				consume();
				t = peek(1);
			}
		} else {
			//  
			while (nest > 0 || t != kTokenCloseParen) {
				if (t.type() == Token::kEndOfFile) {
					error(t, kBadMacroArgumentError);
					return nullopt;
				}

				if (t == kTokenOpenParen) {
					nest++;
				} else if (t == kTokenCloseParen) {
					nest--;
				}

				if (read_tokens) {
					read_tokens->push_back(t);
				}
				arg.push_back(t);

				consume();
				t = peek(1);
			}
		}
		args.push_back(shrink_ws_tokens(arg));

		if (t == kTokenComma) {
			if (read_tokens) {
				read_tokens->push_back(t);
			}
			match(",");
			t = peek(1);

			++comma;
		}
	}

	Token close_paren = peek(1);
	if (read_tokens) {
		read_tokens->push_back(close_paren);
	}
	match(")");

	//  最後の引数が空の場合には上のループで追加されないので、ここで空を追加する。
	if (!macro.params().empty() && (args.size() == comma) && args.size() == macro.params().size() - 1) {
		args.push_back({});
	}

	if (args.size() > kMinSpecMacroArguments) {
		info(close_paren, kMinSpecMacroArgumentsWarning, kMinSpecMacroArguments, args.size());
	}

	if (macro.has_va_args()) {
		const auto must_params = macro.params().size() - 1;

		if (args.size() == must_params) {
			//  これに関する記述がどこか分からない。
			warning(close_paren, kVaArgsRequiresAtLeastOneArgument);
		} else if (args.size() < must_params) {
			error(close_paren, kUnmatchedNumberOfArguments, args.size(), macro.params().size());
			return nullopt;
		}
	} else {
		if (args.size() != macro.params().size()) {
			error(close_paren, kUnmatchedNumberOfArguments, args.size(), macro.params().size());
			return nullopt;
		}
	}

	return args;
}

//void Preprocessor::pp_tokens(bool output) {
//}

void Preprocessor::new_line() {
	Token::Type t = peek(1).type();

	if (t == Token::kNewLine) {
		match(Token::kNewLine);
	} else if (t == Token::kEndOfFile) {
		match(Token::kEndOfFile);
	} else {
		error(peek(1), __func__);
	}
}

void Preprocessor::skip_directive_line(TokenList* skipped_tokens /* = nullptr */) {
	const Token* t = &peek(1);
	while (!t->is_eol()) {
		if (skipped_tokens) {
			skipped_tokens->push_back(*t);
		}

		consume();
		t = &peek(1);
	}
}

void Preprocessor::skip_ws(TokenList* read_tokens) {
	const Token* t = &peek(1);
	while (t->is_ws()) {
		if (read_tokens && t->type() == Token::kWhiteSpace) {
			read_tokens->push_back(*t);
		}
		consume();
		t = &peek(1);
	}
}

void Preprocessor::skip_ws_and_nl(TokenList* read_tokens, bool* broken) {
	bool b = false;

	const Token* t = &peek(1);
	while (t->is_ws_nl()) {
		if (read_tokens && t->type() == Token::kWhiteSpace) {
			read_tokens->push_back(*t);
		}
		if (t->type() == Token::kNewLine) {
			b = true;
		}

		consume();
		t = &peek(1);
	}

	if (broken != nullptr) {
		*broken = b;
	}
}

void Preprocessor::match(Token::Type type) {
	if (peek(1).type() == type) {
		consume();
	} else {
		error(peek(1), __func__);
	}
}

void Preprocessor::match(const std::string& seq) {
	if (peek(1).string() == seq) {
		consume();
	} else {
		error(peek(1), __func__);
	}
}

void Preprocessor::match(const char* seq) {
	if (peek(1).string() == seq) {
		consume();
	} else {
		error(peek(1), __func__);
	}
}

std::string Preprocessor::execute_stringize(const Macro::ArgList& args, Macro::ArgList::size_type first, Macro::ArgList::size_type last) {
	string result;

	result += "\"";
	for (auto i = first; i < last; i++) {
		const auto& arg = args[i];
		for (const auto& t : arg) {
			if (t.type() == Token::kStringLiteral || t.type() == Token::kCharacterConstant) {
				result += escape_string(t.string());
			} else {
				result += t.string();
			}
		}
		if ((i + 1) < last) {
			assert(false && "可変引数は 1つにまとめるようにしたので、このループには入ることは無い。");
			result += ", ";
		}
	}
	result += "\"";

	return result;
}

bool Preprocessor::execute_include(const std::string& header_name, const Token& header_name_token) {
	const bool include_source_dir = (header_name[0] == '"');
	const string name = header_name.substr(1, header_name.length() - 2);

	//
	//string cd = ::get_current_dir();
	string path;
	string source_dir = (*sources_.top()).parent_dir();
	ifstream next_input;

	if (include_source_dir) {
		if (!source_dir.empty()) {
			path = source_dir + kPathDelimiter + name;
		} else {
			path = name;
		}
		path = normalize_path(path);

		if (file_exists(path_string(path))) {
			DEBUG(header_name_token, "Try open \"" + path + "\"");
			next_input.open(path_string(path));
		}
	}
	if (!next_input.is_open()) {
		for (const auto& dir : include_dirs_) {
			path = dir + kPathDelimiter + name;
			path = normalize_path(path);

			if (file_exists(path_string(path))) {
				DEBUG(header_name_token, "Try open <" + path + ">");
				next_input.open(path_string(path));
				break;
			}
		}
	}

	if (!next_input.is_open()) {
		fatal_error(header_name_token, kNoSuchFileError, name.c_str());
		return false;
	}

	DEBUG(header_name_token, "Open file " + path);
	included_files_++;
	if (included_files_ > kMinSpecSourceFileInclusion) {
		info(header_name_token, kMinSpecSourceFileInclusionWarning, kMinSpecSourceFileInclusion, included_files_);
	}
	preprocessing_file(&next_input, path);
	included_files_--;

	return true;
}

bool Preprocessor::execute_define() {
	skip_ws();

	Token name = peek(1);
	if (name.type() != Token::kIdentifier) {
		error(name, kInvalidMacroName);
		skip_directive_line();
	} else {
		match(Token::kIdentifier);

		//  NOTE: マクロ名の直後に空白やコメントを挟まずに括弧が
		//   有る場合のみ、関数型マクロとして解釈する。なので、ここでは
		//   空白を読み飛ばしてはならない。
		bool func_type = (peek(1) == kTokenOpenParen);
		if (!func_type) {
			optional<TokenList> replist = replacement_list(Macro::kObjectLike, Macro::kNoParams);
			if (!replist.has_value()) {
				skip_directive_line();
			} else {
				add_macro(name, *replist);
			}
		} else {
			optional<Macro::ParamList> params = read_macro_params();
			if (!params.has_value()) {
				skip_directive_line();
			} else {
				if (params.value().size() > kMinSpecMacroParameters) {
					info(name, kMinSpecMacroParametersWarning, kMinSpecMacroParameters, params.value().size());
				}

				optional<TokenList> replist = replacement_list(Macro::kFunctionLike, *params);
				if (!replist.has_value()) {
					skip_directive_line();
				} else {
					add_macro(name, *params, *replist);
				}
			}
		}
		skip_ws();
	}
	new_line();

	return true;
}

bool Preprocessor::execute_undef() {
	skip_ws();

	Token name = peek(1);
	if (name.type() != Token::kIdentifier) {
		skip_directive_line();
		new_line();

		error(name, kMustBeMacroName);
		return false;
	}

	match(Token::kIdentifier);
	skip_ws();
	TokenList extra_tokens;
	skip_directive_line(&extra_tokens);
	if (!extra_tokens.empty()) {
		new_line();
		error(extra_tokens.front(), kRedundantTokens);
		return false;
	}
	new_line();

	remove_macro(name);

	return true;
}

bool Preprocessor::execute_pragma(const TokenList& tokens, const Token& location) {
	if (tokens.empty()) {
		//  空のプラグマは何もしない。
		return true;
	//} else if (tokens[0].string() == "STDC") {
	//    "FP_CONTRACT"
	//    "FENV_ACCESS"
	//    "CX_LIMITED_RANGE"
	//    "ON" "OFF" "DEFAULT"
	} else {
		info(location, kPragmaIsIgnoredWarning, Token::concat_string(tokens).c_str());
		return false;
	}
}

//void Preprocessor::output_text(const std::string& text) {
//	output_text(text.c_str());
//}

void Preprocessor::output_text(const char* text) {
	fprintf(output_, "%s", text);
#if !defined(NDEBUG)
	fflush(output_);
#endif
}

void Preprocessor::output_error(const char* format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf_s(error_output_, format, args);
	va_end(args);
#if !defined(NDEBUG)
	fflush(error_output_);
#endif
}

void Preprocessor::output_log(DiagLevel level, const Token& token, const char* format, va_list args) {
	assert(0 <= level && level <= DiagLevel::kMaxLevel);

	if (level > diag_level_) {
		return;
	}

	uint32_t l;
	size_t c;
	if (token.type() != Token::kNull) {
		l = token.line();
		c = token.column();
	} else {
		if (!sources_.empty()) {
			Source& src = *sources_.top();
			l = src.line;
			c = src.scanner->column();
		} else {
			l = 0;
			c = 0;
		}
	}
	string s = current_source_path();

	fprintf(error_output_, "%s:%s:%" PRIu32 ":%zu:", s.c_str(), kLevelTag[level], l, c);
	vfprintf(error_output_, format, args);
	fprintf(error_output_, "\n");
#if !defined(NDEBUG)
	fflush(error_output_);
#endif
}

void Preprocessor::debug(const Token& token, const char* format, ...) {
	va_list args;

	va_start(args, format);
	output_log(DiagLevel::kDebug, token, format, args);
	va_end(args);
}

void Preprocessor::debug(const Token& token, const std::string& message) {
	debug(token, "%s", message.c_str());
}

void Preprocessor::info(const Token& token, const char* format, ...) {
	va_list args;

	va_start(args, format);
	output_log(DiagLevel::kInfo, token, format, args);
	va_end(args);
}

void Preprocessor::info(const Token& token, const std::string& message) {
	info(token, "%s", message.c_str());
}

void Preprocessor::warning(const Token& token, const char* format, ...) {
	va_list args;

	va_start(args, format);
	output_log(DiagLevel::kWarning, token, format, args);
	va_end(args);

	warning_count_++;
}

void Preprocessor::warning(const Token& token, const std::string& message) {
	warning(token, "%s", message.c_str());
}

void Preprocessor::error(const Token& token, const char* format, ...) {
	va_list args;

	va_start(args, format);
	output_log(DiagLevel::kError, token, format, args);
	va_end(args);

	error_count_++;
}

void Preprocessor::error(const Token& token, const std::string& message) {
	error(token, "%s", message.c_str());
}

void Preprocessor::fatal_error(const Token& token, const char* format, ...) {
	va_list args;

	va_start(args, format);
	output_log(DiagLevel::kFatalError, token, format, args);
	va_end(args);

	exit(EXIT_FAILURE);
}

void Preprocessor::fatal_error(const Token& token, const std::string& message) {
	fatal_error(token, "%s", message.c_str());
}

std::string Preprocessor::current_source_path() {
	if (sources_.empty()) {
		return "<init>";
	} else {
		return (*sources_.top()).path;
	}
}

void Preprocessor::current_source_path(const std::string& value) {
	assert(!sources_.empty());
	(*sources_.top()).path = value;
}

uint32_t Preprocessor::current_source_line_number() {
	if (sources_.empty()) {
		return 0;
	} else {
		return (*sources_.top()).line;
	}
}

void Preprocessor::current_source_line_number(uint32_t value) {
	assert(!sources_.empty());
	(*sources_.top()).reset_line_number(value);
}

void Preprocessor::add_predefined_macro(const std::string& name, const std::string& value, const Token::Type type) {
	macros_.insert({ name, make_shared<Macro>(name, value, type) });
	predef_macro_names_.push_back(name);
}

bool Preprocessor::is_valid_macro_name(const Token& name) {
	if (is_predefined_macro_name(name.string())) {
		error(name, kPredefinedMacroNameError, name.string().c_str());
		return false;
	}
	if (name == kTokenOpPragma) {
		error(name, kOpPragmaUsedAsMacroName);
		return false;
	}
	if (name == kTokenVaArgs) {
		error(name, kVaArgsIdentifierUsageError);
		return false;
	}
	if (name == kTokenVaOpt) {
		error(name, kVaOptIdentifierUsageError);
		return false;
	}

	if (name.string().find("__STDC_") == 0) {
		warning(name, kStdcReservedMacroName);
	}

	return true;
}

MacroPtr Preprocessor::add_macro(const Token& name, const TokenList& replist) {
	if (!is_valid_macro_name(name)) {
		return nullptr;
	}

	auto it = macros_.find(name.string());
	if (it != macros_.end()) {
		auto m = it->second;
		if (m->is_function() || (m->replist() != replist)) {
			warning(name, kMacroRedefinitionWarning, name.string().c_str(), m->source().c_str(), m->line(), m->column());
		}
		m->reset(replist, current_source_path(), name);
		DEBUG(name, "[REDEF] %s", macro_def_string(Macro::kObjectLike, name, Macro::kNoParams, replist).c_str());

		return m;
	} else {
		auto result = macros_.insert(
				{ name.string(), make_shared<Macro>(name.string(), replist, current_source_path(), name) });
		if (!result.second) {
			fatal_error(name, __func__ /* ロジックエラーかメモリーが足りないか？ */);
		}
		DEBUG(name, "[DEF] %s", macro_def_string(Macro::kObjectLike, name, Macro::kNoParams, replist).c_str());

		auto inserted = *result.first;
		return inserted.second;
	}
}

MacroPtr Preprocessor::add_macro(const Token& name, const Macro::ParamList& params, const TokenList& replist) {
	if (!is_valid_macro_name(name)) {
		return nullptr;
	}

	auto it = macros_.find(name.string());
	if (it != macros_.end()) {
		auto m = it->second;
		if (m->is_object() || (m->params() != params) || (m->replist() != replist)) {
			warning(name, kMacroRedefinitionWarning, name.string().c_str(), m->source().c_str(), m->line(), m->column());
		}
		m->reset(params, replist, current_source_path(), name);
		DEBUG(name, "[REDEF] %s", macro_def_string(Macro::kFunctionLike, name, params, replist).c_str());

		return m;
	} else {
		auto result = macros_.insert(
				{ name.string(), make_shared<Macro>(name.string(), params, replist, current_source_path(), name) });
		if (!result.second) {
			fatal_error(name, __func__ /* ロジックエラー */);
		}
		DEBUG(name, "[DEF] %s", macro_def_string(Macro::kFunctionLike, name, params, replist).c_str());

		auto inserted = *result.first;
		return inserted.second;
	}
}

std::string Preprocessor::macro_def_string(Macro::Form form, const Token& name, const Macro::ParamList& params, const TokenList& replist) {
	string buf;
	buf.reserve(name.string().length() + 1 + params.size() * 16 + 2 + replist.size() * 32);

	buf += name.string();
	if (form == Macro::kFunctionLike) {
		buf += "(";
		if (!params.empty()) {
			//buf += params.front();
			buf += params[0];
			for (auto p = next(params.begin()); p != params.end(); ++p) {
				buf += ",";
				buf += *p;
			}
		}
		buf += ")";
	}
	buf += "=";
	for (const auto& t : replist) {
		buf += t.string();
	}

	return buf;
}

void Preprocessor::remove_macro(const Token& name) {
	if (!is_valid_macro_name(name)) {
		return;
	}

	auto it = macros_.find(name.string());
	if (it == macros_.end()) {
		warning(name, kUndefineNondefinedMacroWarning, name.string().c_str());
	} else {
		macros_.erase(it);
		DEBUG(name, "[UNDEF] %s", name.string().c_str());
	}
}

const MacroPtr Preprocessor::find_macro(const std::string& name) {
	auto it = macros_.find(name);
	if (it == macros_.end()) {
		return nullptr;
	}
	auto m = it->second;
	if (name == "__FILE__") {
		m->reset({ Token(quote_string(current_source_path()), Token::kStringLiteral) }, "", kTokenNull);
	} else if (name == "__LINE__") {
		m->reset({ Token(to_string(current_source_line_number()), Token::kPpNumber)}, "", kTokenNull);
	}

	return m;
}

void Preprocessor::print_macros() {
	for (const auto& kv : macros_) {
		auto m = kv.second;
		fprintf(error_output_, "%s", m->name().c_str());
		if (m->is_object()) {
			fprintf(error_output_, ": ");
		} else {
			fprintf(error_output_, "%s", kTokenOpenParen.string().c_str());
			for (auto it = m->params().begin(); it != m->params().end(); ++it) {
				if (it != m->params().begin()) {
					fprintf(error_output_, "%s", kTokenComma.string().c_str());
				}
				fprintf(error_output_, "%s", it->c_str());
			}
			fprintf(error_output_, "%s: ", kTokenCloseParen.string().c_str());
		}

		for (const auto& r : m->replist()) {
			fprintf(error_output_, "%s", r.string().c_str());
		}
		fprintf(error_output_, "\n");
	}
}

}   //  namespace pp
