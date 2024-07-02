#include <preprocessor/preprocessor.h>

#include <array>
#include <cstdarg>
#include <cstring>
#include <format>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

#include "util/logger.h"
#include "util/utility.h"

using namespace lib::util;
using namespace std;

namespace {

using namespace pp;

std::vector<std::tuple<std::string, pp::TokenType>> directives = {
    { "include", TokenType::kInclude },
    { "embed", TokenType::kEmbed },
    { "define", TokenType::kDefine },
    { "undef", TokenType::kUndef },
    { "if", TokenType::kIf },
    { "ifdef", TokenType::kIfdef },
    { "ifndef", TokenType::kIfndef },
    { "elif", TokenType::kElif },
    { "elifdef", TokenType::kElifdef },
    { "elifndef", TokenType::kElifndef },
    { "else", TokenType::kElse },
    { "endif", TokenType::kEndif },
    { "error", TokenType::kError },
    { "warning", TokenType::kWarning },
    { "line", TokenType::kLine },
    { "pragma", TokenType::kPragma },
};

pp::TokenType as_directive(const Token& token) {
    auto it = binary_find(
            begin(directives), end(directives), tuple{ token.string(), TokenType::kNull },
            [](auto&& lhs, auto&& rhs) {
                return get<0>(lhs) < get<0>(rhs);
            });
    if (it != end(directives)) {
        return get<1>(*it);
    } else {
        return token.type();
    }
}

inline
bool is_if_group_directive(pp::TokenType type) {
    switch (type) {
    case TokenType::kIf:
    case TokenType::kIfdef:
    case TokenType::kIfndef:
        return true;
    default:
        return false;
    }
}

inline
bool is_elif_group_directive(pp::TokenType type) {
    switch (type) {
    case TokenType::kElif:
    case TokenType::kElifdef:
    case TokenType::kElifndef:
        return true;
    default:
        return false;
    }
}

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
    assert(t.type() == TokenType::kWhiteSpace || t.type() == TokenType::kComment);
    return Token(Token::kTokenValueASpace, t.line(), t.column());
}

TokenList shrink_ws_tokens(const TokenList& ts) {
    TokenList result;
    result.reserve(ts.size());

    for (const auto& t : ts) {
        if (t.type() == TokenType::kNewLine) {
            // skip
        } else if (t.type() == TokenType::kComment || t.type() == TokenType::kWhiteSpace) {
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

const Operator kOperatorOpenParen = { OperatorId::kOpenParen, "(", 254, 0 };
const Operator kOperatorCloseParen = { OperatorId::kCloseParen, ")", 255, 0 };
const Operator kOperatorUnknown = { OperatorId::kUnknown, "", 0, 0 };
const Operator kOperatorPlus = { OperatorId::kPlus, "+", 2, 1 };
const Operator kOperatorMinus = { OperatorId::kMinus, "-", 2, 1 };

std::vector<Operator> operators = {
    kOperatorUnknown,
    kOperatorCloseParen,
    kOperatorOpenParen,
    //{ kOpComma, ",", 15, 2 },
    { OperatorId::kColon, ":", 13, 3 },
    { OperatorId::kCond, "?", 13, 1 },
    { OperatorId::kOr, "||", 12, 2 },
    { OperatorId::kAnd, "&&", 11, 2 },
    { OperatorId::kBitOr, "|", 10, 2 },
    { OperatorId::kBitXor, "^", 9, 2 },
    { OperatorId::kBitAnd, "&", 8, 2 },
    { OperatorId::kEq, "==", 7, 2 },
    { OperatorId::kNeq, "!=", 7, 2 },
    { OperatorId::kLt, "<", 6, 2 },
    { OperatorId::kGt, ">", 6, 2 },
    { OperatorId::kLeq, "<=", 6, 2 },
    { OperatorId::kGeq, ">=", 6, 2 },
    { OperatorId::kShl, "<<", 5, 2 },
    { OperatorId::kShr, ">>", 5, 2 },
    { OperatorId::kAdd, "+", 4, 2 },
    { OperatorId::kSub, "-", 4, 2 },
    { OperatorId::kMul, "*", 3, 2 },
    { OperatorId::kDiv, "/", 3, 2 },
    { OperatorId::kMod, "%", 3, 2 },
    //{ OperatorId::kPlus, "+", 2, 1 },
    //{ OperatorId::kMinus, "-", 2, 1 },
    { OperatorId::kNot, "!", 2, 1 },
    { OperatorId::kCompl, "~", 2, 1 },
};

struct OperatorLess {
    bool operator()(const Operator& lhs, const Operator& rhs) const {
        return lhs.sequence < rhs.sequence;
    }

    bool operator()(const Operator& lhs, const std::string& rhs) const {
        return lhs.sequence < rhs;
    }

    bool operator()(const std::string& lhs, const Operator& rhs) const {
        return lhs < rhs.sequence;
    }
};

const Operator& string_to_op(const std::string& s) {
    auto it = lower_bound(operators.begin(), operators.end(), s, OperatorLess());
    if (it != operators.end() && !(s < it->sequence)) {
        return *it;
    } else {
        return kOperatorUnknown;
    }
}

String replace_string(const String& s, const String& old_seq, const String& new_seq) {
    String result = s;
    String::size_type i = 0;

    while ((i = result.find(old_seq, i)) != String::npos) {
        result.replace(i, old_seq.length(), new_seq);
        i += old_seq.length();
    }

    return result;
}

//  TODO: std::filesystem、これ以外も。
String normalize_path(const String& path) {
    String result = path;
    String::size_type i;

#if HOST_PLATFORM == PLATFORM_WINDOWS
    result = replace_string(result, T_("/"), kPathDelimiter);
#endif
    result = replace_string(result, kPathDelimiter + kPathDelimiter, kPathDelimiter);

    i = 0;
    do {
        String cur = T_(".") + kPathDelimiter;
        i = result.find(cur, i);
        if (i != String::npos) {
            if (i == 0 || result[i - 1] != T_('.')) {
                result.replace(i, cur.length(), T_(""));
            }
            i += 2;
        }
    } while (i != String::npos);

    return result;
}

std::string to_short_pp_parameter_name(const std::string& name) {
    constexpr string_view two_underscores = "__";

    if (name.length() <= (two_underscores.length() * 2)) {
        return name;
    }
    if (!name.starts_with(two_underscores) || !name.ends_with(two_underscores)) {
        return name;
    }

    // アンダースコアを取り除いた結果が識別子になっていない可能性が有るが、そういったものを
    // パラメーター名として用意することは無く、execute_embedでも単に無視されるだけなので
    // 何か実害が有るまでは気にしないでおく。
    return name.substr(two_underscores.length(), name.length() - two_underscores.length() * 2);
}

// 標準の属性
const std::map<std::string, std::string> kSupportedAttributes = {
    //{ "nodiscard",    "202003L" },
    //{ "maybe_unused", "202106L" },
    //{ "deprecated",   "201904L" },
    //{ "fallthrough",  "201910L" },
    //{ "noreturn",     "202202L" },
    //{ "_Noreturn",    "202202L" },  // OBSOLESCENT
    //{ "reproducible", "202207L" },
    //{ "unsequenced",  "202207L" },
};

bool is_digit_sequence(const std::string& s) {
    if (!isdigit(s[0])) {
        return false;
    }

    int prev = 0;
    for (auto c : s) {
        if (!(c == '\'' || isdigit(c))) {
            return false;
        }
        if (prev == '\'' && c == '\'') {
            return false;
        }
        prev = c;
    }

    return true;
}

std::string strip_digit_separator(std::string_view s) {
    string result;
    if (s.empty()) {
        return result;
    }

    result.reserve(s.length());
    for (auto c : s) {
        if (c != '\'') {
            result.append(1, c);
        }
    }
    return result;
}

}   // anonymous namespace

namespace pp {

const StringView kNoInputError = T_("入力ファイルが指定されていない。\n");
const StringView kNoSuchFileError = T_("ファイルが開けない: {}\n");
const StringView kFileOutputError = T_("出力に失敗した。\n");
const StringView kErrorFileOutputError = T_("エラー出力に失敗した。\n");

constexpr char kIdentPragma[] = "_Pragma";
constexpr char kIdentDefined[] = "defined";
constexpr char kIdentVaArgs[] = "__VA_ARGS__";
constexpr char kIdentVaOpt[] = "__VA_OPT__";
constexpr char kIdentHasCAttribute[] = "__has_c_attribute";
constexpr char kIdentHasInclude[] = "__has_include";
constexpr char kIdentHasEmbed[] = "__has_embed";
constexpr char kPunctEllipsis[] = "...";
constexpr char kKeywordTrue[] = "true";

constexpr char kIdentStdcEmbedNotFound[] = "__STDC_EMBED_NOT_FOUND__";
constexpr char kIdentStdcEmbedFound[] = "__STDC_EMBED_FOUND__";
constexpr char kIdentStdcEmbedEmpty[] = "__STDC_EMBED_EMPTY__";

// static
MacroPtr Macro::create_macro(const std::string& name, const TokenList& replist, const std::string& source, const Token& name_token) {
    return make_shared<CreateHelper<Macro>>(name, replist, source, name_token);
}

// static
MacroPtr Macro::create_macro(const std::string& name, const Macro::ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token) {
    return make_shared<CreateHelper<Macro>>(name, params, replist, source, name_token);
}

// static
MacroPtr Macro::create_macro(const std::string& name, const std::string& value, const TokenType type) {
    return make_shared<CreateHelper<Macro>>(name, value, type);
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
    form_ = MacroForm::kObjectLike;
    expantion_method_ = get_expantion_method(MacroForm::kObjectLike, name_, replist);
    params_ = Macro::kNoParams;
    replist_ = replist;
    has_va_args_ = false;
    source_ = source;
    line_ = name_token.line();
    column_ = name_token.column();
}

void Macro::reset(const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token) {
    form_ = MacroForm::kFunctionLike;
    expantion_method_ = get_expantion_method(MacroForm::kFunctionLike, name_, replist);
    params_ = params;
    replist_ = replist;
    has_va_args_ = (params.empty()) ? false : (params.back() == kPunctEllipsis);
    source_ = source;
    line_ = name_token.line();
    column_ = name_token.column();
}

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

Macro::Macro(const std::string& name, const std::string& value, const TokenType type) {
    name_ = name;
    is_predefined_ = true;
    reset({ Token(value, type) }, "", kTokenNull);
}

MacroExpantionMethod Macro::get_expantion_method(MacroForm /*form*/, const std::string& name, const TokenList& replist) {
    if (name == kIdentPragma) {
        return MacroExpantionMethod::kOpPragma;
    } else if (name == kIdentVaOpt) {
        return MacroExpantionMethod::kVaOpt;
    } else if (name == kIdentHasCAttribute) {
        return MacroExpantionMethod::kOpHasCAttribute;
    } else if (name == kIdentHasInclude) {
        return MacroExpantionMethod::kOpHasInclude;
    } else if (name == kIdentHasEmbed) {
        return MacroExpantionMethod::kOpHasEmbed;
    } else {
        bool directly = true;
        for (const auto& t : replist) {
            if (t.type() == TokenType::kIdentifier ||
                t.type() == TokenType::kHash ||
                t.type() == TokenType::kHashHash) {
                directly = false;
                break;
            }
        }
        return directly ? MacroExpantionMethod::kDirectlyCopyable : MacroExpantionMethod::kNormal;
    }
}


IncludeSpec::IncludeSpec(const std::string& header_name)
    : header_name_(header_name) {
}

// IncludeSpec::~IncludeSpec() = default;

std::string IncludeSpec::header_name() const {
    return header_name_.substr(1, header_name_.length() - 2);
}

bool IncludeSpec::is_double_quoted_form() const {
    return header_name_[0] == '"';
}


Preprocessor::Preprocessor(const Options& opts, Diagnostics& diag, SourceFileStack& sources)
    : opts_(opts)
    , diag_(diag)
    , sources_(sources)
    , include_dirs_()
    , diag_level_(DiagLevel::kWarning)
    , clock_start_()
    , clock_end_()
    , stream_stack_()
    , output_(&cout)
    , output_buffer_()
    , output_file_()
    , error_output_(&cerr)
    , error_output_buffer_()
    , error_file_()
    , macros_()
    , predef_macro_names_()
    , used_macro_names_()
    , included_files_()
    , rescan_count_()
{
    clock_start_ = clock();
}

Preprocessor::~Preprocessor() {
    try {
        cleanup();
    } catch (...) {
        // IGNORE
    }
}

bool Preprocessor::has_error() {
    return diag_.error_count() > 0;
}

int Preprocessor::run() {
    //  TODO: コマンドラインで指定されたもの以外でインクルードパスを追加するならここで。

    // include_dirs_への追加は検索順序に関わる。
    // 先にシステム (環境変数)、次にユーザー (Iオプション)の順になっていなければならない。
    const vector<String>& sys_dirs = opts_.system_include_dirs();
    for (const auto& d : sys_dirs) {
        include_dirs_.push_back(IncludeDir{ IncludeDir::kSystem, d });
    }
    const vector<String>& dirs = opts_.additional_include_dirs();
    for (const auto& d : dirs) {
        include_dirs_.push_back(IncludeDir{ IncludeDir::kUser, d });
    }

    //
    String err_path = opts_.error_log_filepath();
    if (err_path.empty()) {
        //error_output_buffer_.resize(4 * 1024);
        //cerr.rdbuf()->pubsetbuf(error_output_buffer_.data(), error_output_buffer_.size());
        error_output_ = &cerr;
    } else {
        error_file_ = make_shared<ofstream>(path_string(err_path), ios_base::binary);
        if (!*error_file_) {
            log_error(kNoSuchFileError, err_path.c_str());
            return 1;
        }
        error_output_ = error_file_.get();
        Logger::instance().set_output_stream(error_file_);
    }
    diag_.set_output(error_output_);

    //
    String out_path = opts_.output_filepath();
    if (out_path.empty()) {
        //output_buffer_.resize(64 * 1024);
        //cout.rdbuf()->pubsetbuf(output_buffer_.data(), output_buffer_.size());
        output_ = &cout;
    } else {
        output_file_.open(path_string(out_path), ios_base::binary);
        if (!output_file_) {
            log_error(kNoSuchFileError, out_path.c_str());
            return 1;
        }
        output_ = &output_file_;
    }

    //
    String in_path = opts_.input_filepath();
    if (in_path.empty()) {
        log_error(kNoInputError);
        return 1;
    }

    Path in_full_path;
    Path in_parent_path;
    istream* in;
    ifstream in_file;
    if (in_path != T_("-")) {
        in_full_path = filesystem::absolute(in_path);
        in_file.open(in_full_path, ios_base::binary);
        if (!in_file.is_open()) {
            log_error(kNoSuchFileError, in_path.c_str());
            return 1;
        }
        in_parent_path = in_full_path.parent_path();
        in = &in_file;
    } else {
        in_full_path = in_path;
        in_parent_path = filesystem::current_path();
        in = &cin;
    }

    prepare_predefined_macro();
    preprocessing_file(in, internal_string(in_full_path), IncludeDir{ IncludeDir::kSource, internal_string(in_parent_path) });
    //log_info("{} errors, {} warnings.\n", error_count_, warning_count_);

    if (!cleanup()) {
        return 1;
    }

    return has_error() ? EXIT_FAILURE : 0;
}

bool Preprocessor::cleanup() {
    diag_.set_output(nullptr);

    if (output_) {
        output_->flush();
        output_ = nullptr;
    }
    if (output_file_.is_open()) {
        output_file_.close();
    }

    if (error_output_) {
        error_output_->flush();

        // TODO: いつもの簡易プロファイラー的なものにする。
        clock_end_ = clock() - clock_start_;
        log_info(T_("elapsed: {}"), clock_end_);
        error_output_->flush();

        error_output_ = nullptr;
    }
    if (error_file_ && error_file_->is_open()) {
        error_file_->close();
    }

    return true;
}

void Preprocessor::prepare_predefined_macro() {
    time_t now;
    time(&now);

    static constexpr int kTmBaseYear = 1900;
    static constexpr char month[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    tm t;
#if HOST_PLATFORM == PLATFORM_WINDOWS
    if (errno_t e = localtime_s(&t, &now); e != 0) {
        raise_generic_error("localtime_s", e);
    }
#else
    if (localtime_r(&now, &t) == nullptr) {
        raise_generic_error("localtime_r", errno);
    }
#endif
    const auto date = format("{:3} {:2} {:4}", month[t.tm_mon], t.tm_mday, kTmBaseYear + t.tm_year);
    const auto time = format("{:02}:{:02}:{:02}", t.tm_hour, t.tm_min, t.tm_sec);

    //  定義済みマクロをここで追加する。
    predef_macro_names_.clear();

    add_predefined_macro("__DATE__",         quote_string(date), TokenType::kStringLiteral);
    add_predefined_macro("__FILE__",         quote_string(""),   TokenType::kStringLiteral);
    add_predefined_macro("__LINE__",         "0",                TokenType::kPpNumber);
    add_predefined_macro("__STDC__",         "1",                TokenType::kPpNumber);
    add_predefined_macro("__STDC_HOSTED__",  "0",                TokenType::kPpNumber);
    add_predefined_macro("__STDC_VERSION__", "201112L",          TokenType::kPpNumber);
    add_predefined_macro("__TIME__",         quote_string(time), TokenType::kStringLiteral);

    //
    //add_predefined_macro("__STDC_ISO_10646__",       "yyyymmL", TokenType::kPpNumber);
    //add_predefined_macro("__STDC_MB_MIGHT_NEQ_WC__", "1",       TokenType::kPpNumber);
    //add_predefined_macro("__STDC_UTF_16__",          "1",       TokenType::kPpNumber);
    //add_predefined_macro("__STDC_UTF_32__",          "1",       TokenType::kPpNumber);

    //
    //add_predefined_macro("__STDC_ANALYZABLE__",      "1",       TokenType::kPpNumber);
    //add_predefined_macro("__STDC_IEC_559__",         "1",       TokenType::kPpNumber);    // obsolescent
    //add_predefined_macro("__STDC_IEC_559_COMPLEX__", "1",       TokenType::kPpNumber);    // obsolescent
    //add_predefined_macro("__STDC_IEC_60559_BFP__",     "yyyymmL", TokenType::kPpNumber);
    //add_predefined_macro("__STDC_IEC_60559_DFP__",     "yyyymmL", TokenType::kPpNumber);
    //add_predefined_macro("__STDC_IEC_60559_COMPLEX__", "yyyymmL", TokenType::kPpNumber);
    //add_predefined_macro("__STDC_LIB_EXT1__",        "201109L", TokenType::kPpNumber);
    add_predefined_macro("__STDC_NO_ATOMICS__", "1", TokenType::kPpNumber);
    add_predefined_macro("__STDC_NO_COMPLEX__", "1", TokenType::kPpNumber);
    add_predefined_macro("__STDC_NO_THREADS__", "1", TokenType::kPpNumber);
    add_predefined_macro("__STDC_NO_VLA__",     "1", TokenType::kPpNumber);

    add_predefined_macro(kIdentStdcEmbedNotFound, "0", TokenType::kPpNumber);
    add_predefined_macro(kIdentStdcEmbedFound,    "1", TokenType::kPpNumber);
    add_predefined_macro(kIdentStdcEmbedEmpty,    "2", TokenType::kPpNumber);

    //  マクロではないが、マクロ定義の対象にならない？ようなのでここで追加する。
    predef_macro_names_.push_back(kIdentDefined);
    predef_macro_names_.push_back(kIdentHasCAttribute);
    predef_macro_names_.push_back(kIdentHasInclude);
    predef_macro_names_.push_back(kIdentHasEmbed);

    //  仕様的な定義済みマクロはここまでとして、後の検索の為にソートしておく。
    sort(predef_macro_names_.begin(), predef_macro_names_.end());

    //  _Pragma演算子はマクロとして扱う。definedと違って括弧が必要なので、所々で特別扱いしないで済む。
    //  実際の処理は独自のメソッドで行われるので置換リストは適当に。
    {
        auto op_pragma = Macro::create_macro(
                kIdentPragma,
                Macro::ParamList{ "content" },
                TokenList{ Token("content", TokenType::kIdentifier) }, "", kTokenNull);
        auto result = macros_.insert({ op_pragma->name(), op_pragma });
        if (!result.second) {
            fatal_error(kTokenNull, as_internal(__func__) /* ロジックエラーかメモリーが足りないか？ */);
        }
    }

    //  __VA_OPT__は特別なマクロとして扱う。
    {
        auto op_va_opt = Macro::create_macro(
                kIdentVaOpt,
                Macro::ParamList{ "content" },
                TokenList{ Token("content", TokenType::kIdentifier) }, "", kTokenNull);
        auto result = macros_.insert({ op_va_opt->name(), op_va_opt });
        if (!result.second) {
            fatal_error(kTokenNull, as_internal(__func__));
        }
    }

    // __has_c_attributeは特別なマクロとして扱う。
    {
        auto op_has_c_attribute = Macro::create_macro(
            kIdentHasCAttribute,
            Macro::ParamList{ "content" },
            TokenList{ Token("content", TokenType::kIdentifier) }, "", kTokenNull);
        auto result = macros_.insert({ op_has_c_attribute->name(), op_has_c_attribute });
        if (!result.second) {
            fatal_error(kTokenNull, as_internal(__func__));
        }
    }

    // __has_includeは特別なマクロとして扱う。
    {
        auto op_has_include = Macro::create_macro(
            kIdentHasInclude,
            Macro::ParamList{ "header_name" },
            TokenList{}, "", kTokenNull);
        auto result = macros_.insert({ op_has_include->name(), op_has_include });
        if (!result.second) {
            fatal_error(kTokenNull, as_internal(__func__));
        }
    }

    // __has_embed
    // パラメーターは複数指定できるが、コンマ区切りではないので 1つだけ。read_macro_argsを使わず特別扱い。
    {
        auto op_has_embed = Macro::create_macro(
            kIdentHasEmbed,
            Macro::ParamList{ "header_name" },
            TokenList{}, "", kTokenNull);
        auto result = macros_.insert({ op_has_embed->name(), op_has_embed });
        if (!result.second) {
            fatal_error(kTokenNull, as_internal(__func__));
        }
    }

    //  NOTE: 処理系定義のマクロが必要になった時には、ここで追加する。
    //        その際は、predef_macro_names_とは別にリストを用意して、warning扱い
    //        すると良いかも？

    for (const auto& op : opts_.macro_operations()) {
        switch (op.operation()) {
        case MacroDefinitionOperationType::kDefine: {
            auto def{op.operand()};
            auto i = def.find(T_('='));
            if (i == decltype(def)::npos) {
                //  定義が無いか、或いは、コマンドライン引数を引用符で囲って、#defineの様に
                //  空白で区切っているかもしれない。後者の場合、明確にサポートするものでは
                //  ないが、そのまま通している。
            } else {
                //  #defineでの処理を使えるようにするため、空白に置き換える。
                //  名前と置換リストが空白で区切られており、置換リストに '='が含まれる場合を
                //  考慮していない。
                def[i] = T_(' ');
            }

            //  トークンのマッチングの都合上、改行を加えてから定義を実行している。
            SourceString source(source_string(def), opts_, diag_, sources_);
            TokenStream stream(source);
            push_stream(stream);
            execute_define();
            pop_stream();
            break;
        }
        case MacroDefinitionOperationType::kUndefine: {
            const auto& name = op.operand();
            SourceString source(source_string(name), opts_, diag_, sources_);
            TokenStream stream(source);
            push_stream(stream);
            execute_undef();
            pop_stream();
            break;
        }
        }
    }
}

void Preprocessor::preprocessing_file(std::istream* input, const String& path, const IncludeDir& include_dir) {
    SourceFile source(*input, path, include_dir, opts_, diag_, sources_);
    TokenStream stream(source);
    push_stream(stream);

    Group root(true, TokenType::kNull);

    //group()?;
    while (peek(1).type() != TokenType::kEndOfFile) {
        group(current_source(), root);
    }

    pop_stream();
}

void Preprocessor::group(SourceFile& source, Group& group) {
    //group_part();
    //group(); group_part();

    GroupScope scope(source, group);

    if (source.num_groups() >= kMinSpecConditionalInclusion) {
        info(peek(1), kMinSpecConditionalInclusionWarning, kMinSpecConditionalInclusion, source.num_groups());
    }

    do {
        if (!group_part()) {
            break;
        }
    } while (peek(1).type() != TokenType::kEndOfFile);
}

bool Preprocessor::group_part() {
    TokenList ws_tokens;
    skip_ws(&ws_tokens);

    if (peek(1).type() == TokenType::kHash) {
        match(TokenType::kHash);
        skip_ws();

        SourceFile& src = current_source();
        TokenType cur_group = src.current_group()->type;
        Token dir_token = peek(1);
        TokenType dir = as_directive(dir_token);

        if (((cur_group == TokenType::kIf)       && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kIfdef)    && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kIfndef)   && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kElif)     && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kElifdef)  && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kElifndef) && (is_elif_group_directive(dir) || dir == TokenType::kElse || dir == TokenType::kEndif)) ||
            ((cur_group == TokenType::kElse)     && (dir == TokenType::kEndif))) {
            return false;
        }

        if (is_if_group_directive(dir)) {
            if_section();
        } else if (
            dir == TokenType::kInclude ||
            dir == TokenType::kEmbed ||
            dir == TokenType::kDefine ||
            dir == TokenType::kUndef ||
            dir == TokenType::kLine ||
            dir == TokenType::kError ||
            dir == TokenType::kWarning ||
            dir == TokenType::kPragma ||
            dir == TokenType::kNewLine) {
            control_line(dir);
        } else {
            if (src.condition_level() == 0) {
                if (is_elif_group_directive(dir) ||
                    dir == TokenType::kElse ||
                    dir == TokenType::kEndif) {
                    error(dir_token, kNoCorrespondingIfError, dir_token.string());
                }
            } else {
                if (cur_group == TokenType::kElse && is_elif_group_directive(dir)) {
                    error(dir_token, kElifGroupAfterElseError, dir_token.string());
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

    SourceFile& src = current_source();
    ConditionScope condition_scope(src);
    bool processed = false;

    Token if_token = peek(1);
    TokenType dir = as_directive(if_token);
    if (is_if_group_directive(dir)) {
        processed = if_group();
    } else {
        fatal_error(if_token, as_internal(__func__) /* ロジックエラーっぽい */);
    }

    //  ここに来た時点で "#"は consume済み。
    skip_ws();

    TokenType elif_dir = as_directive(peek(1));
    if (is_elif_group_directive(elif_dir)) {
        //processed = elif_groups(processed);
        Token t;
        do {
            processed = elif_group(processed) || processed;
            elif_dir = as_directive(peek(1));
        } while (is_elif_group_directive(elif_dir));
    }

    skip_ws();
    if (as_directive(peek(1)) == TokenType::kElse) {
        else_group(processed);
    }

    skip_ws();
    if (as_directive(peek(1)) == TokenType::kEndif) {
        endif_line();
    } else {
        error(if_token, kUnterminatedIfError, if_token.line());
    }
}

TokenList Preprocessor::make_constant_expression() {
    skip_ws();

    bool bad_expr = false;
    TokenList expr;
    Token t;
    while (!peek(1).is_eol() && !bad_expr) {
        t = peek(1);

        if (t.type() != TokenType::kIdentifier) {
            consume();      // XXX: これ。
            expr.push_back(t);
            continue;
        }

        bool needs_header_name = (t.string() == kIdentHasInclude) || (t.string() == kIdentHasEmbed);
        if (needs_header_name) {
            current_source().scanner_hint(ScannerHint::kHeaderName);
        }

        consume();      // XXX: これも。

        if (needs_header_name) {
            current_source().scanner_hint(ScannerHint::kInitial);
        }

        if (t.string() == kIdentVaArgs) {
            error(t, kVaArgsIdentifierUsageError);
            bad_expr = true;
            continue;
        }

        if (t.string() == kIdentDefined) {
            skip_ws();
            bool open = (peek(1).type() == TokenType::kLeftParenthesis);
            if (open) {
                match(TokenType::kLeftParenthesis);
                skip_ws();
            }

            Token macro_name = peek(1);
            consume();
            if (macro_name.type() != TokenType::kIdentifier) {
                error(peek(1), as_internal(__func__) /* マクロ名が指定されていなかった？構文がおかしいので失敗 */);
                bad_expr = true;
            } else {
                MacroPtr m = find_macro(macro_name.string());
                const Token& v = (m != nullptr) ? kTokenPpNumberOne : kTokenPpNumberZero;
                expr.push_back(v);

                if (open) {
                    skip_ws();
                    if (peek(1).type() != TokenType::kRightParenthesis) {
                        error(peek(1), as_internal(__func__) /* 閉じ括弧が無い。構文がおかしいので失敗 */);
                        bad_expr = true;
                    }
                    match(TokenType::kRightParenthesis);
                }
            }
        } else if (t.string() == kIdentHasEmbed) {
            skip_ws();

            if (peek(1).type() != TokenType::kLeftParenthesis) {
                expr.push_back(kTokenPpNumberZero);
            } else {
                match(TokenType::kLeftParenthesis);
                skip_ws();

                MacroPtr m = find_macro(t.string());
                if (!m) {
                    bad_expr = true;
                    fatal_error(t, as_internal(__func__) /* マクロ追加忘れ。 */);
                }

                int depth = 0;
                TokenList tokens;
                t = peek(1);
                while (!(depth == 0 && t.type() == TokenType::kRightParenthesis) && !t.is_eol()) {
                    if (t.type() == TokenType::kLeftParenthesis) {
                        ++depth;
                    }
                    if (depth > 0 && t.type() == TokenType::kRightParenthesis) {
                        --depth;
                    }
                    tokens.push_back(move(t));

                    consume();
                    t = peek(1);
                }
                if (t.type() != TokenType::kRightParenthesis) {
                    error(peek(1), as_internal(__func__) /* 閉じ括弧が無い。構文がおかしいので失敗 */);
                    bad_expr = true;
                } else {
                    match(TokenType::kRightParenthesis);
                }

                if (!bad_expr) {
                    Macro::ArgList args;
                    args.push_back(move(tokens));

                    TokenList replaced;
                    expand(*m, args, replaced);
                    expr.push_back(replaced.front());
                }
            }
        } else {
            MacroPtr m = find_macro(t.string());
            if (m == nullptr) {
                // true、falseがマクロとして定義されているバージョンであれば find_macroで見つかるはず。
                // キーワードとして定義されているバージョンであればここで処理する。
                // マクロ定義が存在しない識別子は "0"になり、"false"はいずれにしても "0"なので、ここでは
                // "true"とだけ比較している。
                if (t.string() == kKeywordTrue) {
                    expr.push_back(kTokenPpNumberOne);
                } else {
                    expr.push_back(kTokenPpNumberZero);
                }
            } else {
                TokenList replaced;
                if (!m->is_function()) {
                    DEBUG(t, T_("[START]: {}"), m->name());
                    expand(*m, Macro::kNoArgs, replaced);
                } else {
                    skip_ws();

                    if (peek(1).type() != TokenType::kLeftParenthesis) {
                        warning(t, kFuncTypeMacroUsageWarning);
                        bad_expr = true;
                    } else {
                        auto args = read_macro_args(*m, nullptr);
                        if (!args.has_value()) {
                            bad_expr = true;
                        } else {
                            DEBUG(t, T_("[START]: {}"), m->name());
                            expand(*m, *args, replaced);
                        }
                    }
                }

                replace_stream(move(replaced));
            }
        }
    }

    if (expr.empty() || bad_expr) {
        skip_directive_line();
        expr.clear();
        expr.push_back(kTokenPpNumberZero);
    }

    return expr;
}

target_intmax_t Preprocessor::constant_expression(const TokenList& expr_tokens, const Token& dir_token) {
    //  TODO: make_constant_expressionの中身をここに移して、ここの中身は calculator.cppにしたい。

#if !defined(NDEBUG)
    auto expr_string = Token::concat_string(expr_tokens);
#endif

    SourceTokenList source(expr_tokens);
    TokenStream stream(source);
    push_stream(stream);

    stack<Operator> ops;
    stack<target_intmax_t> nums;
    bool bad_expr = false;
    bool unary = true;

    ops.push(kOperatorOpenParen);

    skip_ws();
    Token t = peek(1);
    while (!t.is_eol() && !bad_expr) {
        switch (t.type()) {
        case TokenType::kPpNumber: {
            auto& s = t.string();
            match(TokenType::kPpNumber);

            target_intmax_t n = 0;
            if (!parse_int(s, &n)) {
                error(t, kConstantNumberIsNotAIntegerError, s);
                bad_expr = true;
            }
            nums.push(n);
            unary = false;
            break;
        }
        case TokenType::kCharacterConstant: {
            //  TODO: 全くまともに作っていない。kPpNumberの場合と同じようにするには辛いので、Scanner側で
            //      数値を求めておきたい。
            auto& s = t.string();
            match(TokenType::kCharacterConstant);

            auto i = s.find('\'');
            int c = (s[i + 1] & 0xff);
            nums.push(c);
            unary = false;
            break;
        }
        case TokenType::kPunctuator:
        case TokenType::kLeftParenthesis:
        case TokenType::kRightParenthesis: {
            auto& opstr = t.string();
            match(t.type());

            Operator op = string_to_op(opstr);

            if (unary) {
                switch (op.id) {
                case OperatorId::kAdd:
                    op = kOperatorPlus;
                    break;

                case OperatorId::kSub:
                    op = kOperatorMinus;
                    break;

                case OperatorId::kCompl:
                case OperatorId::kNot:
                case OperatorId::kOpenParen:
                case OperatorId::kCloseParen:
                    //  差し替えは必要ない。
                    break;

                case OperatorId::kUnknown:
                    //  下の　kInvalidOperatorErrorにするのでここでは何もしない。
                    break;

                default:
                    error(t, kInvalidConstantExpressionError);
                    bad_expr = true;
                    break;
                }
            }
            if (op.id == OperatorId::kUnknown) {
                error(dir_token, kInvalidOperatorError, opstr);
                bad_expr = true;
            }

            if (!bad_expr) {
                if (op.id == OperatorId::kOpenParen) {
                    ops.push(kOperatorOpenParen);
                    unary = false;
                } else {
                    if (!unary) {
                        calc(ops, nums, op);
                    }
                    if (op.id == OperatorId::kCloseParen) {
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

    pop_stream();

    if (bad_expr) {
        return 0;
    }

    calc(ops, nums, kOperatorCloseParen);

    if (nums.size() != 1) {
        error(dir_token, as_internal(__func__));
        return 0;
    }

    target_intmax_t result = nums.top();
    nums.pop();

    return result;
}

void Preprocessor::calc(stack<Operator>& ops, stack<target_intmax_t>& nums, const Operator& next_op) {
    if (ops.empty()) {
        return;
    }

    int nest = 0;
    while (!ops.empty() &&
        ((next_op.id != OperatorId::kCloseParen && next_op.precedence > ops.top().precedence) ||
        (next_op.id == OperatorId::kCloseParen && !(nest == 0 && ops.top().id == OperatorId::kOpenParen)))) {
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
            case OperatorId::kPlus:
                result = l;
                break;
            case OperatorId::kMinus:
                result = -l;
                break;
            case OperatorId::kCompl:
                result = ~l;
                break;
            case OperatorId::kNot:
                result = (l == 0) ? 1 : 0;
                break;
            case OperatorId::kCond:
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
            case OperatorId::kOr:
                result = (l != 0) || (r != 0);
                break;
            case OperatorId::kAnd:
                result = (l != 0) && (r != 0);
                break;
            case OperatorId::kBitOr:
                result = l | r;
                break;
            case OperatorId::kBitXor:
                result = l ^ r;
                break;
            case OperatorId::kBitAnd:
                result = l & r;
                break;
            case OperatorId::kEq:
                result = l == r;
                break;
            case OperatorId::kNeq:
                result = l != r;
                break;
            case OperatorId::kLt:
                result = l < r;
                break;
            case OperatorId::kGt:
                result = l > r;
                break;
            case OperatorId::kLeq:
                result = l <= r;
                break;
            case OperatorId::kGeq:
                result = l >= r;
                break;
            case OperatorId::kShl:
                result = l << r;
                break;
            case OperatorId::kShr:
                result = l >> r;
                break;
            case OperatorId::kAdd:
                result = l + r;
                break;
            case OperatorId::kSub:
                result = l - r;
                break;
            case OperatorId::kMul:
                result = l * r;
                break;
            case OperatorId::kDiv:
                result = l / r;
                break;
            case OperatorId::kMod:
                result = l % r;
                break;
            //case OperatorId::kComma:
            //    result = r;
            //    break;
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
            case OperatorId::kColon:
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
            case OperatorId::kUnknown:
                break;
            case OperatorId::kOpenParen:
                nest--;
                break;
            case OperatorId::kCloseParen:
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

    //"#" "if" constant_expression(); match(TokenType::kNewLine); group() ? ;
    //"#" "ifdef" match(TokenType::kIdentifier); match(TokenType::kNewLine); group() ? ;
    //"#" "ifndef" match(TokenType::kIdentifier); match(TokenType::kNewLine); group() ? ;
    target_intmax_t result;
    SourceFile& src = current_source();
    Token dir_token = peek(1);
    TokenType dir = as_directive(dir_token);
    Group* parent = src.current_group();

    if (!parent->processing) {
        skip_directive_line();
        new_line();
        result = 0;
    } else {
        if (dir == TokenType::kIf) {
            match("if");
            result = constant_expression(make_constant_expression(), dir_token);
        } else if (dir == TokenType::kIfdef) {
            match("ifdef");
            skip_ws();

            Token name_token = peek(1);
            if (name_token.type() != TokenType::kIdentifier) {
                error(name_token, kNoIdentifierSpecifiedError, dir_token.string());
                skip_directive_line();
                result = 0;
            } else {
                auto& name = name_token.string();
                match(TokenType::kIdentifier);
                skip_ws();

                if (name == kIdentVaArgs) {
                    error(name_token, kVaArgsIdentifierUsageError);
                }
                result = (find_macro(name) != nullptr);
            }
        } else if (dir == TokenType::kIfndef) {
            match("ifndef");
            skip_ws();

            Token name_token = peek(1);
            if (name_token.type() != TokenType::kIdentifier) {
                error(name_token, kNoIdentifierSpecifiedError, dir_token.string());
                skip_directive_line();
                result = 0;
            } else {
                auto& name = name_token.string();
                match(TokenType::kIdentifier);
                skip_ws();

                if (name == kIdentVaArgs) {
                    error(name_token, kVaArgsIdentifierUsageError);
                }
                result = (find_macro(name) == nullptr);
            }
        } else {
            fatal_error(dir_token, as_internal(__func__) /* ロジックエラー */);
        }

        new_line();
    }

    Group g((result != 0) && parent->processing, dir);
    group(src, g);

    return g.processing;
}

bool Preprocessor::elif_groups(bool /*processed*/) {
    //elif_group();
    //elif_groups(); elif_group();

    //do {
    //    processed = elif_group(processed) || processed;
    //} while (peek(1).type() == TokenType::kHash && as_directive(peek(2)) == TokenType::kElif);

    //return processed;

    fatal_error(kTokenNull, T_("elif_groupsは if_sectionにおいて展開されている。"));
}

bool Preprocessor::elif_group(bool processed) {
    //"#" "elif" constant_expression(); new_line(); group() ? ;
    skip_ws();

    target_intmax_t result;
    SourceFile& src = current_source();
    Token dir_token = peek(1);
    TokenType dir = as_directive(dir_token);
    Group* parent = src.current_group();

    if (!parent->processing || processed) {
        skip_directive_line();
        result = 0;
    } else {
        if (dir == TokenType::kElif) {
            match("elif");
            skip_ws();

            result = constant_expression(make_constant_expression(), dir_token);
        } else if (dir == TokenType::kElifdef) {
            match("elifdef");
            skip_ws();

            Token name_token = peek(1);
            if (name_token.type() != TokenType::kIdentifier) {
                error(name_token, kNoIdentifierSpecifiedError, dir_token.string());
                skip_directive_line();
                result = 0;
            } else {
                auto& name = name_token.string();
                match(TokenType::kIdentifier);
                skip_ws();

                if (name == kIdentVaArgs) {
                    error(name_token, kVaArgsIdentifierUsageError);
                }
                result = (find_macro(name) != nullptr);
            }
        } else if (dir == TokenType::kElifndef) {
            match("elifndef");
            skip_ws();

            Token name_token = peek(1);
            if (name_token.type() != TokenType::kIdentifier) {
                error(name_token, kNoIdentifierSpecifiedError, dir_token.string());
                skip_directive_line();
                result = 0;
            } else {
                auto& name = name_token.string();
                match(TokenType::kIdentifier);
                skip_ws();

                if (name == kIdentVaArgs) {
                    error(name_token, kVaArgsIdentifierUsageError);
                }
                result = (find_macro(name) == nullptr);
            }
        } else {
            fatal_error(dir_token, as_internal(__func__) /* ロジックエラー */);
        }
    }
    new_line();

    Group g(parent->processing && !processed && (result != 0), dir);
    group(src, g);

    return g.processing;
}

void Preprocessor::else_group(bool processed) {
    //"#" "else" new_line(); group()?;
    skip_ws();
    match("else");
    skip_ws();
    new_line();

    SourceFile& src = current_source();
    Group* parent = src.current_group();
    Group g(parent->processing && !processed, TokenType::kElse);
    group(src, g);
}

void Preprocessor::endif_line() {
    //"#" "endif" new_line();
    skip_ws();
    match("endif");
    skip_ws();
    new_line();
}

void Preprocessor::control_line(TokenType directive) {
    assert(directive == as_directive(peek(1)));

    //"#" "include" pp_tokens(TokenType::kHeaderName); new_line();
    //"#" "define" match(TokenType::kIdentifier); replacement_list(); new_line();

    //"#" "define" match(TokenType::kIdentifier); lparen();
    //while (? ? ? ) {
    //    identifier_list();
    //}
    //match(')');
    //replacement_list();
    //new_line();

    //"#" "define" match(TokenType::kIdentifier); lparen();
    //match("...");
    //match(')');
    //replacement_list();
    //new_line();

    //"#" "define" match(TokenType::kIdentifier); lparen();
    //identifier_list();
    //match(',');
    //match("...");
    //match(')');
    //replacement_list();
    //new_line();

    //"#" "undef" identifier(); new_line();
    //"#" "line" pp_tokens(); new_line();
    //"#" "error" pp_tokens() ? ; new_line();
    //"#" "warning" pp_tokens() ? ; new_line();
    //"#" "pragma" pp_tokens() ? ; new_line();
    //"#" new_line();

    SourceFile& src = current_source();
    Group* cur_group = src.current_group();

    if (!cur_group->processing) {
        skip_directive_line();
        new_line();

        return;
    }

    switch (directive) {
    case TokenType::kInclude: {
        src.scanner_hint(ScannerHint::kHeaderName);
        match("include");
        src.scanner_hint(ScannerHint::kInitial);
        skip_ws();

        Token header_name_token = peek(1);
        string header_name;
        if (header_name_token.type() == TokenType::kHeaderName) {
            header_name = header_name_token.string();
            match(TokenType::kHeaderName);
        } else if (header_name_token.type() == TokenType::kIdentifier) {
            match(TokenType::kIdentifier);

            TokenList expanded;
            MacroPtr m = find_macro(header_name_token.string());
            if (m == nullptr) {
                error(header_name_token, as_internal(__func__));
            } else {
                if (!m->is_function()) {
                    expand(*m, Macro::kNoArgs, expanded);
                } else {
                    skip_ws();
                    if (peek(1).type() != TokenType::kLeftParenthesis) {
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
                header_name.clear();
            } else {
                header_name = Token::concat_string(expanded);
            }
        } else {
            skip_directive_line();
        }
        if (!peek(1).is_eol()) {
            header_name.clear();
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
    case TokenType::kEmbed: {
        src.scanner_hint(ScannerHint::kHeaderName);
        match("embed");
        src.scanner_hint(ScannerHint::kInitial);
        skip_ws();

        // 規格的にどうなのか分からないが、とりあえず他のディレクティブとは異なり、一気に展開しておく。
        // もしかすると、今後、まず展開無しでリソースとパラメーターの解析を試みて、それに失敗すれば展開して
        // 再度、解析を試みるようになるかもしれない。何か変わるにせよ、変わらないにせよ、いずれは、他の
        // ディレクティブもこのように変更して統一したい。その場合、恐らく動作が変わる。
        TokenList expanded = expand_directive_line();
        SourceTokenList source(expanded);
        TokenStream stream(source);
        push_stream(stream);

        Token resource_id_token = peek(1);
        if (resource_id_token.type() == TokenType::kStringLiteral) {
            // NOTE: 文字列であっても、execute_embed内でエスケープシーケンスなどは解釈されない。
            resource_id_token = Token(resource_id_token.string(), TokenType::kHeaderName);

            match(TokenType::kStringLiteral);
        } else if (resource_id_token.string() == "<") {
            Token t = resource_id_token;
            TokenList ts;
            while (t.string() != ">" && !t.is_eol()) {
                ts.push_back(move(t));

                consume();
                t = peek(1);
            }
            if (t.string() == ">") {
                ts.push_back(move(t));
                resource_id_token = Token(Token::concat_string(ts), TokenType::kHeaderName);

                match(">");
            }
        } else if (resource_id_token.type() == TokenType::kHeaderName) {
            match(TokenType::kHeaderName);
        }

        if (resource_id_token.type() == TokenType::kHeaderName) {
            skip_ws();

            bool succeeded = true;
            EmbedSpec spec(resource_id_token.string());
            if (peek(1).type() == TokenType::kIdentifier) {
                succeeded = embed_parameter_sequence(&spec);
            }

            if (!peek(1).is_eol()) {
                skip_directive_line();
            }

            if (succeeded) {
                execute_embed(spec, false);
            }
        } else {
            error(peek(1), kNoEmbedResourceIdentifierError);
            skip_directive_line();
        }

        pop_stream();

        new_line();
        break;
    }
    case TokenType::kDefine: {
        match("define");
        execute_define();
        break;
    }
    case TokenType::kUndef: {
        match("undef");
        execute_undef();
        break;
    }
    case TokenType::kLine: {
        match("line");
        skip_ws();

        TokenList expanded = expand_directive_line();
        SourceTokenList source(expanded);
        TokenStream stream(source);
        push_stream(stream);

        bool parsed = false;
        Token line_token = peek(1);
        optional<string> path;
        if (line_token.type() != TokenType::kPpNumber) {
            error(line_token, kLineNeedsDecimalConstantError);
            skip_directive_line();
        } else {
            match(TokenType::kPpNumber);
            skip_ws();

            Token name_token = peek(1);
            if (name_token.is_eol()) {
                // 行だけ。
                parsed = true;
            } else if (name_token.type() == TokenType::kStringLiteral) {
                // 行とパス。
                match(TokenType::kStringLiteral);
                skip_ws();

                path = name_token.string();
                parsed = true;
            } else {
                error(peek(1), kRedundantTokens);
                skip_directive_line();
            }
        }

        pop_stream();

        new_line();

        if (parsed) {
            execute_line(line_token.string(), path);
        }
        break;
    }
    case TokenType::kError: {
        Token t = peek(1);
        match("error");
        skip_ws();

        TokenList ts;
        skip_directive_line(&ts);
        ts = shrink_ws_tokens(ts);
        new_line();

        error(t, T_("#error {}"), internal_from_source(Token::concat_string(ts)));
        break;
    }
    case TokenType::kWarning: {
        Token t = peek(1);
        match("warning");
        skip_ws();

        TokenList ts;
        skip_directive_line(&ts);
        ts = shrink_ws_tokens(ts);
        new_line();

        warning(t, T_("#warning {}"), internal_from_source(Token::concat_string(ts)));
        break;
    }
    case TokenType::kPragma: {
        match("pragma");
        skip_ws();

        Token first_token = peek(1);
        TokenList ts;
        skip_directive_line(&ts);
        new_line();

        ts = shrink_ws_tokens(ts);
        //if (first_token.string() == kIdentStdc) {
        //}
        execute_pragma(ts, first_token);
        break;
    }
    case TokenType::kNewLine: {
        new_line();
        break;
    }
    default:
        fatal_error(peek(1), as_internal(__func__) /* ロジックエラー */);
        break;
    }
}

void Preprocessor::text_line(const TokenList& ws_tokens) {
    //  pp_tokens()* new_line()

    SourceFile& src = current_source();
    Group* cur_group = src.current_group();
    //bool output = cur_group->processing;

    //while (peek(1).type() != TokenType::kNewLine) {
    //    pp_tokens(output);
    //}
    //if (output) {
    //    output_text("\n");
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

        if (t.type() == TokenType::kComment) {
            continue;
        }
        if (t.type() != TokenType::kIdentifier) {
            output_text(t.string());
            continue;
        }
        if (t.string() == kIdentVaArgs) {
            error(t, kVaArgsIdentifierUsageError);
            output_text(t.string());
            continue;
        }
        if (t.string() == kIdentVaOpt) {
            error(t, kVaOptIdentifierUsageError);
            output_text(t.string());
            continue;
        }
        if (t.string() == kIdentHasCAttribute || t.string() == kIdentHasInclude || t.string() == kIdentHasEmbed) {
            error(t, kConditionalInclusionOperatorUsageError, t.string());
            output_text(t.string());
            continue;
        }

        MacroPtr m = find_macro(t.string());
        if (m == nullptr) {
            output_text(t.string());
            continue;
        }

        bool dont_replace = false;
        bool dont_rescan = false;
        TokenList expanded;
        if (!m->is_function()) {
            DEBUG(t, T_("[START]: {}"), m->name());
            dont_rescan = expand(*m, Macro::kNoArgs, expanded);
        } else {
            bool broken = false;
            TokenList ws;
            skip_ws_and_nl(&ws, &broken);

            if (peek(1).type() != TokenType::kLeftParenthesis) {
                warning(t, kFuncTypeMacroUsageWarning);
                expanded = move(ws);
                dont_replace = true;

                if (broken && (peek(1).type() == TokenType::kHash)) {
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
                    DEBUG(t, T_("[START]: {}"), m->name());
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

    Token nl = peek(1);
    new_line();
    if (nl.type() == TokenType::kNewLine) {
        output_text(nl.string());
    }
}

const TokenList& Preprocessor::get_expanded_arg(size_t n, const TokenList& arg, Macro::ArgList& cache) {
    if (!arg.empty() && cache[n].empty()) {
        SourceTokenList source(arg);
        TokenStream stream(source);
        push_stream(stream);
        scan(cache[n]);
        pop_stream();

        DEBUG(kTokenNull, T_("{}{}: {} --> {}"), Indent::tab(), n, Token::concat_string(arg), Token::concat_string(cache[n]));
    }

    return cache[n];
}

bool Preprocessor::expand(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded) {
#if !defined(NDEBUG)
    Indent indent;
#endif

    Macro::ArgList expanded_args(macro_args.size());
    macro_invocation_stack_.push_back({ &macro, &macro_args, &expanded_args });
    auto ord = enum_ordinal(macro.expantion_method());
    bool dont_rescan = (this->*expantion_methods_[ord])(macro, macro_args, result_expanded);
    macro_invocation_stack_.pop_back();

    return dont_rescan;
}

bool Preprocessor::expand_directly_copyable(const Macro& macro, const Macro::ArgList& /*macro_args*/, TokenList& result_expanded) {
    result_expanded = macro.replist();
    return true;
}

bool Preprocessor::expand_normal(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded) {
    // 使われている引数を展開する。
    Macro::ArgList& expanded_args = *macro_invocation_stack_.back().expanded_args;
    const TokenList& list = macro.replist();
    bool used_va_args = false;
    for (auto it = macro.replist().begin(); it != macro.replist().end(); ++it) {
        const Token& t = *it;
        if (t.type() != TokenType::kIdentifier) {
            continue;
        }
        if (macro.has_va_args() && (t.string() == kIdentVaArgs || t.string() == kIdentVaOpt)) {
            used_va_args = true;
            continue;
        }

        auto i = macro.param_index_of(t.string());
        if (i != macro.params().size()) {
            get_expanded_arg(i, macro_args[i], expanded_args);
        }
    }

    // 可変引数を展開する。
    // 引数そのものが使われていなくても (__VA_ARGS__が現れなくても)、__VA_OPT__が有れば、その判断に
    // 利用されるので、やはり展開しておく。
    if (used_va_args) {
        const auto i0 = macro.params().size() - 1;  // マクロの仮引数の数 - 1: 即ち、"..."のオフセット
        const auto end = macro_args.size();
        if (i0 < end) {
            get_expanded_arg(i0, macro_args[i0], expanded_args);
        }
    }

    //  パラメーターのトークンを展開済み引数に置き換える。
    //  引数は必要時に展開される。
    TokenList substituted;
    for (auto it = list.begin(); it != list.end(); ++it) {
        const Token& t = *it;

        if (t.type() == TokenType::kHash && macro.is_function()) {
            substituted.push_back(t);
            substituted.push_back(*++it);
        } else if (t.type() == TokenType::kHashHash) {
            substituted.push_back(t);
            substituted.push_back(*++it);
        } else {
            auto next_it = next(it);
            if (next_it != macro.replist().end() && next_it->type() == TokenType::kHashHash) {
                substituted.push_back(t);
            } else {
                if (t.type() != TokenType::kIdentifier) {
                    substituted.push_back(t);
                } else {
                    if (macro.has_va_args() && t.string() == kIdentVaArgs) {
                        const auto i0 = macro.params().size() - 1;  // マクロの仮引数の数 - 1、即ち、"..."のオフセット
                        const auto end = macro_args.size();
                        if (i0 < end) {
                            const auto& a0 = get_expanded_arg(i0, macro_args[i0], expanded_args);
                            substituted.insert(substituted.end(), a0.begin(), a0.end());
                        }
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
            if (it->type() == TokenType::kHash) {
                auto next_it = next(it);
                if (!(next_it != substituted.end() &&
                    next_it->type() == TokenType::kIdentifier &&
                    (find(macro.params().begin(), macro.params().end(), next_it->string()) != macro.params().end() ||
                        next_it->string() == kIdentVaArgs))) {
                    fatal_error(*next_it, as_internal(__func__) /* マクロ定義時に失敗しているはず */);
                }

                string s;
                if (macro.has_va_args() && next_it->string() == kIdentVaArgs) {
                    s = execute_stringize(macro_args, macro.params().size() - 1, macro_args.size());
                } else {
                    auto i = macro.param_index_of(next_it->string());
                    if (i == macro.params().size()) {
                        fatal_error(*next_it, as_internal(__func__) /* マクロ定義時に失敗しているはず */);
                    }
                    s = execute_stringize(macro_args, i, i + 1);
                }

                it = substituted.erase(it, next(next_it));
                it = substituted.insert(it, Token(s, TokenType::kStringLiteral));
            }
        }
    }

    //  連結(##)を行う。
    for (auto it = substituted.begin(); it != substituted.end(); ) {
        if (it->type() != TokenType::kHashHash) {
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
            istringstream in(l.string() + r.string(), ios_base::binary);
            Scanner scanner(in, opts_.support_trigraphs(), diag_, sources_);
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
                //  TODO: 事前に細かく分けて結合すべきなのかもしれない。有り得るのは id+id=id, id+num=id,
                //        num+num=num, punct+punct=punctの 4パターン。punct同士以外については、ここに来たと
                //        しても、それは結合できたということなので、特に問題は無いだろうか。ただ、numは
                //        あくまでも pp-numberなのでパーサーがエラーにする可能性は残る。
                Token t2 = concat_result.front();
                if (t2.type() == TokenType::kHashHash || t2.is_ws()) {
                    error(r, kGeneratedInvalidPpTokenError2, l.string(), r.string());
                    concat_result.pop_back();
                    concat_result.push_back(Token(t2.string(), TokenType::kNonReplacementTarget));
                }
            } else {
                error(r, kGeneratedInvalidPpTokenError2, l.string(), r.string());
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

    DEBUG(kTokenNull, T_("{} --> {}"), Indent::tab(), Token::concat_string(substituted));

    if (substituted.empty()) {
        result_expanded.clear();
    } else {
        rescan_count_++;
        used_macro_names_.insert(macro.name());

        SourceTokenList source(substituted);
        TokenStream stream(source);
        push_stream(stream);

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
            expanded_args[0][0].type() != TokenType::kStringLiteral) {
        error(macro_args[0][0], kOpPragmaParameterTypeMismatch);
        return true;
    }

    Token pragma_text = expanded_args[0][0];
    auto pragma = destringize(pragma_text.string());

    istringstream in(pragma, ios_base::binary);
    Scanner scanner(in, opts_.support_trigraphs(), diag_, sources_);
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

bool Preprocessor::expand_va_opt(const Macro& /*macro*/, const Macro::ArgList& macro_args, TokenList& result_expanded) {
    if (macro_invocation_stack_.size() < 2) {
        fatal_error(kTokenNull, as_internal(__func__));
    }

    const auto& parent_invocation = macro_invocation_stack_[macro_invocation_stack_.size() - 2];
    if (parent_invocation.macro->params().empty()) {
        fatal_error(kTokenNull, as_internal(__func__));
    }

    // 可変引数が指定されていなければ、結果は空にする(何もしない)。
    const auto parent_i0 = parent_invocation.macro->params().size() - 1;
    const auto parent_end = parent_invocation.args->size();
    if (parent_i0 >= parent_end) {
        return true;
    }

    // 可変引数が有っても、空リストの場合も可変引数は指定されていないものとする。
    const Macro::ArgList* parent_args = parent_invocation.expanded_args;
    if (parent_args->empty() || (*parent_args)[parent_i0].empty()) {
        return true;
    }

    // __VA_OPT__に指定された内容で置き換える。
    if (!macro_args.empty()) {
        result_expanded.insert(result_expanded.end(), macro_args[0].begin(), macro_args[0].end());
    }

    return false;
}

bool Preprocessor::expand_op_has_c_attribute(const Macro& /*macro*/, const Macro::ArgList& macro_args, TokenList& result_expanded) {
    if (macro_args.size() != 1 || macro_args[0].empty()) {
        error(kTokenNull, kOpHasCAttributeNeedsAnAttribute);
        result_expanded.push_back(kTokenPpNumberZero);
        return true;
    }

    Macro::ArgList expanded_args(macro_args.size());
    get_expanded_arg(0, macro_args[0], expanded_args);

    const auto& arg = expanded_args[0];
    SourceTokenList source(arg);
    TokenStream stream(source);
    push_stream(stream);

    string attr_name;
    bool parsed;
    if (peek(1).type() == TokenType::kIdentifier) {
        // 文法的には attribute-tokenだが、先に作った同じ処理の pp_parameter_nameを流用している。
        parsed = pp_parameter_name(&attr_name);
    } else {
        error(peek(1), kOpHasCAttributeNeedsAnAttribute);
        parsed = false;
    }

    if (!peek(1).is_eol()) {
        if (parsed) {
            error(peek(1), kOpHasCAttributeNeedsAnAttribute);
        }

        skip_directive_line();
        parsed = false;
    }

    pop_stream();

    if (parsed) {
        auto it = kSupportedAttributes.find(attr_name);
        if (it != kSupportedAttributes.end()) {
            result_expanded.push_back(Token(it->second, TokenType::kPpNumber));
        } else {
            result_expanded.push_back(kTokenPpNumberZero);
        }
    } else {
        result_expanded.push_back(kTokenPpNumberZero);
    }

    return true;
}



bool Preprocessor::expand_op_has_include(const Macro& /*macro*/, const Macro::ArgList& macro_args, TokenList& result_expanded) {
    Macro::ArgList expanded_args(macro_args.size());

    if (macro_args.size() != 1 || macro_args[0].empty()) {
        error(kTokenNull, kOpHasIncludeParameterTypeMismatchError);
        return true;
    }

    get_expanded_arg(0, macro_args[0], expanded_args);

    auto& arg = expanded_args[0];
    string header_name;
    if (arg.size() == 1 && arg[0].type() == TokenType::kHeaderName) {
        header_name = arg[0].string();
    } else {
        if (!arg.empty() && ((arg[0].string() == "<" && arg[arg.size() - 1].string() == ">") || arg[0].type() == TokenType::kStringLiteral)) {
            header_name = Token::concat_string(arg);
        } else {
            error(macro_args[0][0], kOpHasIncludeParameterTypeMismatchError);
            return true;
        }
    }

    IncludeSpec spec(header_name);
    if (search_include_file(spec, nullptr, nullptr)) {
        result_expanded.push_back(kTokenPpNumberOne);
    } else {
        result_expanded.push_back(kTokenPpNumberZero);
    }

    return true;
}

bool Preprocessor::expand_op_has_embed(const Macro& /*macro*/, const Macro::ArgList& macro_args, TokenList& result_expanded) {
    Macro::ArgList expanded_args(macro_args.size());

    if (macro_args.size() != 1 || macro_args[0].empty()) {
        error(kTokenNull, kOpHasEmbedParameterTypeMismatchError);
        return true;
    }

    get_expanded_arg(0, macro_args[0], expanded_args);

    const auto& arg = expanded_args[0];
    SourceTokenList source(arg);
    TokenStream stream(source);
    push_stream(stream);

    Token resource_id_token = peek(1);
    if (resource_id_token.type() == TokenType::kStringLiteral) {
        // NOTE: 文字列であっても、execute_embed内でエスケープシーケンスなどは解釈されない。
        resource_id_token = Token(resource_id_token.string(), TokenType::kHeaderName);

        match(TokenType::kStringLiteral);
    } else if (resource_id_token.string() == "<") {
        Token t = resource_id_token;
        TokenList ts;
        while (t.string() != ">" && !t.is_eol()) {
            ts.push_back(move(t));

            consume();
            t = peek(1);
        }
        if (t.string() == ">") {
            ts.push_back(move(t));
            resource_id_token = Token(Token::concat_string(ts), TokenType::kHeaderName);

            match(">");
        }
    } else if (resource_id_token.type() == TokenType::kHeaderName) {
        match(TokenType::kHeaderName);
    }

    if (resource_id_token.type() == TokenType::kHeaderName) {
        skip_ws();

        bool succeeded = true;
        EmbedSpec spec(resource_id_token.string());
        if (peek(1).type() == TokenType::kIdentifier) {
            succeeded = embed_parameter_sequence(&spec);
        }

        if (!peek(1).is_eol()) {
            skip_directive_line();
        }

        const char* result_ident = nullptr;
        if (succeeded) {
            auto result = execute_embed(spec, true);
            switch (result) {
            case EmbedResult::kErrorOrUnsupportedParameter:
                result_ident = kIdentStdcEmbedNotFound;
                break;

            case EmbedResult::kComplete:
                result_ident = kIdentStdcEmbedFound;
                break;

            case EmbedResult::kResourceIsEmpty:
                result_ident = kIdentStdcEmbedEmpty;
                break;

            default:
                fatal_error(kTokenNull, as_internal(__func__));
                break;
            }
        }
        if (result_ident) {
            result_expanded.push_back({ result_ident, TokenType::kIdentifier });
        }
    } else {
        error(kTokenNull, kNoEmbedResourceIdentifierError);
        skip_directive_line();
    }

    pop_stream();

    return true;
}

TokenList Preprocessor::expand_directive_line() {
    TokenList tokens;
    skip_directive_line(&tokens);

    SourceTokenList source(tokens);
    TokenStream stream(source);
    push_stream(stream);

    TokenList result;
    scan(result);

    pop_stream();

    return result;
}

bool Preprocessor::search_include_file(const IncludeSpec& include_spec, String* file_path_str, IncludeDir* include_dir) {
    String name = internal_from_source(include_spec.header_name());
    if (name.empty()) {
        return false;
    }
    String path_str;
    IncludeDir result_inc_dir;
    bool exist = false;

    if (include_spec.is_double_quoted_form()) {
        auto source_dir = current_source().parent_dir();
        path_str = source_dir + kPathDelimiter + name;
        path_str = normalize_path(path_str);
        exist = file_exists(path_string(path_str));
        result_inc_dir = IncludeDir{ IncludeDir::kSource, source_dir };
    }

    if (!exist) {
        for (const auto& dir : include_dirs_) {
            path_str = dir.path() + kPathDelimiter + name;
            path_str = normalize_path(path_str);
            exist = file_exists(path_string(path_str));
            if (exist) {
                result_inc_dir = dir;
                break;
            }
        }
    }

    if (exist) {
        if (file_path_str) {
            *file_path_str = move(path_str);
        }
        if (include_dir) {
            *include_dir = move(result_inc_dir);
        }
    }

    return exist;
}

TokenList Preprocessor::substitute_by_arg_if_need(const Macro& macro, const Macro::ArgList& macro_args, const Token& token) {
    if (token.type() != TokenType::kIdentifier) {
        //result.push_back(token);
        return { token };
    }

    TokenList result;
    if (macro.has_va_args() && token.string() == kIdentVaArgs) {
        auto i0 = macro.params().size() - 1;
        const auto end = macro_args.size();
        if (i0 < end) {
            const TokenList& a0 = macro_args[i0];
            //if (a0.empty()) {
            //    result.push_back(kTokenPlaceMarker);
            //} else {
            result.insert(result.end(), a0.begin(), a0.end());
            //}
        }
    } else {
        auto i = macro.param_index_of(token.string());
        if (i == macro.params().size()) {
            result.push_back(token);
        } else {
            const TokenList& a = macro_args[i];
            //if (a.empty()) {
            //    result.push_back(kTokenPlaceMarker);
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

        if (t.type() != TokenType::kIdentifier) {
            result_expanded.push_back(t);
            continue;
        }
        if (t.string() == kIdentVaArgs) {
            // expandで展開されるはずなので、ここで見つかるのはエラーとする。
            error(t, kVaArgsIdentifierUsageError);
            result_expanded.push_back(t);
            continue;
        }
        if (t.string() == kIdentVaOpt) {
            // 可変引数を持つマクロの展開中でなければエラーとする。
            bool context_error = true;
            if (!macro_invocation_stack_.empty()) {
                const Macro* current_invocation = macro_invocation_stack_.back().macro;
                if (current_invocation->has_va_args()) {
                    context_error = false;
                }
            }
            if (context_error) {
                error(t, kVaOptIdentifierUsageError);
                result_expanded.push_back(t);
                continue;
            }
        }
        if (used_macro_names_.find(t.string()) != used_macro_names_.end()) {
            DEBUG(t, T_("{}[USED]: {}"), Indent::tab(), t.string());
            result_expanded.push_back(Token(t.string(), TokenType::kNonReplacementTarget));
            continue;
        }
        MacroPtr m = find_macro(t.string());
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

            if (peek(1).type() != TokenType::kLeftParenthesis) {
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
        MacroForm macro_form, const Macro::ParamList& macro_params) {
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
    int old_error_count = diag_.error_count();
    if (macro_form == MacroForm::kFunctionLike) {
        for (auto it = result.begin(); it != result.end(); it++) {
            if (it->type() == TokenType::kHash) {
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
                if (!found_param && ident->string() != kIdentVaArgs) {
                    error(*ident, kOpStringizeNeedsParameterError);
                    break;
                }
            }
        }
    }

    for (auto it = result.begin(); it != result.end(); it++) {
        if (it->type() == TokenType::kHashHash) {
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

    // 可変引数絡みの識別子を使えない場合に、それが見つかればエラーとする。
    if ((macro_form == MacroForm::kFunctionLike && (macro_params.empty() || (!macro_params.empty() && macro_params.back() != kPunctEllipsis))) ||
        macro_form == MacroForm::kObjectLike) {
        Token va_args;
        Token va_opt;
        for_each(result.begin(), result.end(),
                [&va_args, &va_opt](const auto& t) {
                    if (t.string() == kIdentVaArgs) {
                        va_args = t;
                    } else if (t.string() == kIdentVaOpt) {
                        va_opt = t;
                    }
                });
        if (!va_args.is_null()) {
            error(va_args, kVaArgsIdentifierUsageError);
        }
        if (!va_opt.is_null()) {
            error(va_opt, kVaOptIdentifierUsageError);
        }
    }

    if (diag_.error_count() > old_error_count) {
        return nullopt;
    }

    return result;
}

std::optional<Macro::ParamList> Preprocessor::read_macro_params() {
    if (peek(1).type() != TokenType::kLeftParenthesis) {
        fatal_error(peek(1), as_internal(__func__) /* ロジック間違い */);
        //return nullopt;
    }

    match(TokenType::kLeftParenthesis);
    skip_ws();

    bool bad_params = false;
    vector<string> params;
    set<string> added;
    while (peek(1).type() != TokenType::kRightParenthesis) {
        skip_ws();
        Token id = peek(1);
        consume();

        skip_ws();
        Token delim_or_close = peek(1);
        if (delim_or_close.type() != TokenType::kComma && delim_or_close.type() != TokenType::kRightParenthesis) {
            error(id, kBadMacroParameterFormError);
            bad_params = true;
            break;
        } else {
            if (delim_or_close.type() == TokenType::kComma) {
                match(TokenType::kComma);
            }
        }

        if (id.type() == TokenType::kIdentifier) {
            if (id.string() == kIdentVaArgs) {
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
        } else if (id.type() == TokenType::kEllipsis) {
            params.push_back(id.string());
            if (delim_or_close.type() != TokenType::kRightParenthesis) {
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
    match(TokenType::kRightParenthesis);
    return params;
}

template <class Funcion>
Token Preprocessor::read_macro_arg_loop(TokenList* read_tokens, TokenList& result_arg, Funcion end_condition) {
    bool succeeded = true;
    int nest = 0;
    Token t = peek(1);
    while (nest > 0 || !end_condition(t)) {
        if (t.type() == TokenType::kEndOfFile) {
            error(t, kBadMacroArgumentError);
            succeeded = false;
            break;
        }

        if (t.type() == TokenType::kLeftParenthesis) {
            nest++;
        } else if (t.type() == TokenType::kRightParenthesis) {
            nest--;
        }

        result_arg.push_back(t);

        consume();
        t = peek(1);
    }

    if (read_tokens) {
        read_tokens->insert(read_tokens->end(), result_arg.begin(), result_arg.end());
    }

    return succeeded ? t : kTokenNull;
}

Token Preprocessor::read_macro_arg(TokenList* read_tokens, TokenList& result_arg) {
    return read_macro_arg_loop(read_tokens, result_arg,
            [](const Token& t) {
                return t.type() == TokenType::kComma || t.type() == TokenType::kRightParenthesis;
            });
}

Token Preprocessor::read_macro_arg_at_ellipsis(TokenList* read_tokens, TokenList& result_arg) {
    return read_macro_arg_loop(read_tokens, result_arg,
            [](const Token& t) {
                return t.type() == TokenType::kRightParenthesis;
            });
}

std::optional<Macro::ArgList> Preprocessor::read_macro_args(const Macro& macro, TokenList* read_tokens) {
    Token open_paren = peek(1);
    if (open_paren.type() != TokenType::kLeftParenthesis) {
        fatal_error(peek(1), as_internal(__func__) /* ロジック間違い */);
        //return nullopt;
    }

    if (read_tokens) {
        read_tokens->push_back(open_paren);
    }
    match(TokenType::kLeftParenthesis);

    Macro::ArgList args;
    size_t comma = 0;

    if (macro.name() == kIdentVaOpt || macro.name() == kIdentHasCAttribute) {
        // ここは実際に可変引数である訳ではない。コンマで区切らないトークン列を返す読み取り処理の流用している。
        args.reserve(1);

        TokenList arg;
        if (read_macro_arg_at_ellipsis(read_tokens, arg).is_null()) {
            return nullopt;
        }

        args.push_back(shrink_ws_tokens(arg));
    } else {
        // 正しく引数が指定されていれば、
        // 可変引数を持たない場合: 仮引数の数で領域を予約し、その全ての領域を使う。
        //                        MACRO(a, b, c)が有って、MACRO(a, b, )と使う場合、最後は空リストになる。
        // 可変引数を持つ場合: 仮引数の数で領域を予約するが、可変引数が 1つも無い場合は、コンマが有るか
        //                    どうかで、空リストになるか、空リストではなく要素自体が無いかの 2通りになる。
        //                    MACRO(a, b, ...)が有って、MACRO(a, b, )と使う場合、最後は空リストになるが、
        //                    MACRO(a, b)と使う場合は、可変引数に対応する要素自体が無く、予約領域の最後は
        //                    使われない。
        args.reserve(macro.params().size());

        Token t = peek(1);
        while (t.type() != TokenType::kRightParenthesis) {
            const auto ellipsis_pos = macro.params().size() - 1;
            TokenList arg;

            if (!macro.has_va_args() ||
                (macro.has_va_args() && args.size() < ellipsis_pos)) {
                t = read_macro_arg(read_tokens, arg);
            } else {
                t = read_macro_arg_at_ellipsis(read_tokens, arg);
            }
            if (t.is_null()) {
                return nullopt;
            }

            args.push_back(shrink_ws_tokens(arg));

            if (t.type() == TokenType::kComma) {
                if (read_tokens) {
                    read_tokens->push_back(t);
                }
                match(TokenType::kComma);
                t = peek(1);

                ++comma;
            }
        }

        //  最後の引数が空の場合には上のループで追加されないので、ここで空を追加する。
        if (!macro.params().empty() && (args.size() == comma) && (args.size() == macro.params().size() - 1)) {
            args.push_back({});
        }
    }

    Token close_paren = peek(1);
    if (read_tokens) {
        read_tokens->push_back(close_paren);
    }
    match(TokenType::kRightParenthesis);

    if (args.size() > kMinSpecMacroArguments) {
        info(close_paren, kMinSpecMacroArgumentsWarning, kMinSpecMacroArguments, args.size());
    }

    if (macro.has_va_args()) {
        const auto must_params = macro.params().size() - 1;
        if (args.size() < must_params) {
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
    TokenType t = peek(1).type();

    if (t == TokenType::kNewLine) {
        match(TokenType::kNewLine);
    } else if (t == TokenType::kEndOfFile) {
        match(TokenType::kEndOfFile);
    } else {
        error(peek(1), as_internal(__func__));
    }
}

bool Preprocessor::pp_balanced_token(TokenList* tokens) {
    Token left = peek(1);
    if (is_left_bracket(left.type())) {
        tokens->push_back(left);
        match(left.type());
        skip_ws();

        Token right = peek(1);
        if (!is_matched_bracket(left.type(), right.type())) {
            if (!pp_balanced_token_sequence(left.type(), tokens)) {
                return false;
            }

            if (!is_matched_bracket(left.type(), peek(1).type())) {
                error(left, kUnclosedBracketError);
                return false;
            }
        }

        tokens->push_back(right);
        match(right.type());
        skip_ws();
    } else {
        Token t = peek(1);
        while (!is_bracket(t.type()) && !t.is_eol()) {
            tokens->push_back(t);

            consume();
            t = peek(1);
        }
    }

    return true;
}

bool Preprocessor::pp_balanced_token_sequence(TokenType left_bracket, TokenList* result_tokens) {
    const TokenType right_bracket = get_right_bracket(left_bracket);

    while (peek(1).type() != right_bracket && !peek(1).is_eol()) {
        if (!pp_balanced_token(result_tokens)) {
            return false;
        }
    }

    return !peek(1).is_eol();
}

bool Preprocessor::pp_parameter_name(std::string* result_name) {
    assert(result_name != nullptr);

    *result_name = to_short_pp_parameter_name(peek(1).string());

    match(TokenType::kIdentifier);
    skip_ws();

    if (!(peek(1).type() == TokenType::kPunctuator && peek(1).string() == "::")) {
        return true;
    }

    match(TokenType::kPunctuator);
    skip_ws();

    if (peek(1).type() != TokenType::kIdentifier) {
        error(peek(1), kBadPrefixedEmbedParameterError);
        return false;
    }

    *result_name += "::";
    *result_name += to_short_pp_parameter_name(peek(1).string());

    match(TokenType::kIdentifier);
    skip_ws();

    return true;
}

bool Preprocessor::pp_paramter_clause(EmbedParameter* parameter) {
    Token left = peek(1);
    match(TokenType::kLeftParenthesis);
    skip_ws();

    // pp-parameter-clauseは丸括弧だけなので、ここでは tokensに丸括弧は含めていない。
    TokenList tokens;
    if (peek(1).type() != TokenType::kRightParenthesis) {
        if (!pp_balanced_token_sequence(TokenType::kLeftParenthesis, &tokens)) {
            return false;
        }

        if (peek(1).type() != TokenType::kRightParenthesis) {
            error(left, kUnclosedBracketError);
            return false;
        }
    }
    parameter->set_value(move(tokens));

    match(TokenType::kRightParenthesis);
    skip_ws();

    return true;
}

bool Preprocessor::pp_parameter(EmbedSpec* spec) {
    Token t = peek(1);
    if (t.type() != TokenType::kIdentifier) {
        error(t, kBadEmbedParameterError);
        return false;
    }

    std::string name;
    if (!pp_parameter_name(&name)) {
        return false;
    }

    EmbedParameter param(name);
    if (peek(1).type() == TokenType::kLeftParenthesis) {
        if (!pp_paramter_clause(&param)) {
            return false;
        }
    }

    if (EmbedParameter::is_standard_parameter_name(name)) {
        if (spec->parameter(name).has_value()) {
            // XXX: 今のところ、全ての標準パラメーターは 0個か 1個かだけ。
            error(peek(1), kSameEmbedParameterSpecifiedError, name);
            return false;
        }
        if (!param.has_value()) {
            error(peek(1), kEmbedParameterClauseUnspecifiedError, name);
            return false;
        }
    }

    spec->add_parameter(move(param));

    return true;
}

bool Preprocessor::embed_parameter_sequence(EmbedSpec* spec) {
    Token t;
    while ((t = peek(1)).type() == TokenType::kIdentifier) {
        if (!pp_parameter(spec)) {
            return false;
        }
    }

    return true;
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
        if (read_tokens && t->type() == TokenType::kWhiteSpace) {
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
        // read_tokensで返すトークンはエラーの時だけ使われるので、改行が
        // 入っても問題無い。それはコメントも同様だが、今のところ、コメント
        // 自体は出力しないので、それだけは除外している。
        if (read_tokens && t->type() != TokenType::kComment) {
            read_tokens->push_back(*t);
        }
        if (t->type() == TokenType::kNewLine) {
            b = true;
        }

        consume();
        t = &peek(1);
    }

    if (broken != nullptr) {
        *broken = b;
    }
}

void Preprocessor::match(TokenType type) {
    if (peek(1).type() == type) {
        consume();
    } else {
        error(peek(1), as_internal(__func__));
    }
}

void Preprocessor::match(const std::string& seq) {
    if (peek(1).string() == seq) {
        consume();
    } else {
        error(peek(1), as_internal(__func__));
    }
}

void Preprocessor::match(const char* seq) {
    if (peek(1).string() == seq) {
        consume();
    } else {
        error(peek(1), as_internal(__func__));
    }
}

std::string Preprocessor::execute_stringize(const Macro::ArgList& args, Macro::ArgList::size_type first, Macro::ArgList::size_type last) {
    string result;

    result += "\"";
    for (auto i = first; i < last; i++) {
        const auto& arg = args[i];
        for (const auto& t : arg) {
            if (t.type() == TokenType::kStringLiteral || t.type() == TokenType::kCharacterConstant) {
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
    IncludeSpec spec(header_name);
    String path_str;
    IncludeDir include_dir;
    ifstream next_input;
    if (search_include_file(spec, &path_str, &include_dir)) {
        next_input.open(path_string(path_str), ios_base::binary);
    }

    if (!next_input.is_open()) {
        fatal_error(header_name_token, kNoSuchFileError, spec.header_name().c_str());
        return false;
    }

    DEBUG(header_name_token, T_("Open file {}"), path_str);
    included_files_++;
    if (included_files_ > kMinSpecSourceFileInclusion) {
        info(header_name_token, kMinSpecSourceFileInclusionWarning, kMinSpecSourceFileInclusion, included_files_);
    }
    preprocessing_file(&next_input, path_str, include_dir);
    included_files_--;

    return true;
}

EmbedResult Preprocessor::execute_embed(EmbedSpec& spec, bool has_embed_context) {
    constexpr auto kEmbedElementWidth = TARGET_CHAR_BIT;
    // とりあえずこれで制限する。
    constexpr auto kMaxResourceSizeInBytes = 0x10000;

    // TODO: インクルードパスを流用しない方が良い？リソースパスを指定できるようにした方が良い？
    // TODO: ヘッダーファイルの流用なのをどうにかする。名前だけ変えるとか。
    IncludeSpec i_spec(spec.resource_id());
    String path_str;
    if (!search_include_file(i_spec, &path_str, nullptr)) {
        if (!has_embed_context) {
            fatal_error(peek(1), kEmbedResoruceNotFoundError, spec.resource_id());
        }
        return EmbedResult::kErrorOrUnsupportedParameter;
    }

    Path path = path_string(path_str);
    ifstream resource_file(path, ios_base::binary);
    if (!resource_file) {
        if (!has_embed_context) {
            fatal_error(peek(1), kEmbedResoruceOpeningFailureError, spec.resource_id());
        }
        return EmbedResult::kErrorOrUnsupportedParameter;
    }

    const auto size = filesystem::file_size(path);
    if (size > (numeric_limits<int>::max() >> kEmbedElementWidth)) {
        if (!has_embed_context) {
            error(peek(1), as_internal(__func__) /* 実装上の都合、オーバーフローしそう */);
        }
        return EmbedResult::kErrorOrUnsupportedParameter;
    }

    // TODO: 上のチェックと合わせて、もうすこし、こう、型を考える。
    const int file_size = static_cast<int>(size);
    // この辺の用語の意味を勘違いしているような気がしてならない。
    const int implementation_resource_width = file_size * kEmbedElementWidth;
    int resource_width = 0;     // in bits

    auto limit_ref = spec.parameter(EmbedParameter::kIdentLimit);
    if (limit_ref.has_value()) {
        auto& limit = limit_ref->get();
        for (auto& t : limit.value()) {
            if (t.string() == kIdentDefined) {
                if (!has_embed_context) {
                    error(t, kEmbedUsingDefinedInLimitParameterError);
                }
                return EmbedResult::kErrorOrUnsupportedParameter;
            }
        }

        const int limit_result = constant_expression(limit.value(), peek(1));    // in bytes
        if (limit_result < 0) {
            if (!has_embed_context) {
                error(limit.value().front(), kEmbedLimitParameterLessThan0Error);
            }
            return EmbedResult::kErrorOrUnsupportedParameter;
        } else if (limit_result > kMaxResourceSizeInBytes) {
            if (!has_embed_context) {
                error(limit.value().front(), as_internal(__func__) /* 独自の制限 {kMaxResourceSizeInBytes}以下のリソースしか取り扱えない */);
            }
            return EmbedResult::kErrorOrUnsupportedParameter;
        }

        resource_width = min(implementation_resource_width, kEmbedElementWidth * limit_result);
    } else {
        if (file_size > kMaxResourceSizeInBytes) {
            if (!has_embed_context) {
                error(peek(0), as_internal(__func__) /* 独自の制限 {kMaxResourceSizeInBytes}以下のリソースしか取り扱えない */);
            }
            return EmbedResult::kErrorOrUnsupportedParameter;
        }

        resource_width = implementation_resource_width;
    }

    if ((resource_width % kEmbedElementWidth) != 0) {
        if (!has_embed_context) {
            error(peek(1), kEmbedResourceWidthCanBeDividedByEmbedElementWidth, kEmbedElementWidth);
        }
        return EmbedResult::kErrorOrUnsupportedParameter;
    }

    if (!has_embed_context) {
        // warning(/*パラメーター Xは無視される的な */);
    } else {
        for (auto& p : spec.parameters()) {
            if (!EmbedParameter::is_supported_parameter_name(p.name())) {
                return EmbedResult::kErrorOrUnsupportedParameter;
            }
        }
    }

    if (!has_embed_context) {
        if (resource_width == 0) {
            auto if_empty_ref = spec.parameter(EmbedParameter::kIdentIfEmpty);
            if (if_empty_ref.has_value()) {
                auto& if_empty = if_empty_ref->get();
                for (auto& t : if_empty.value()) {
                    output_text(t.string());
                }
            }
        } else {
            auto prefix_ref = spec.parameter(EmbedParameter::kIdentPrefix);
            if (prefix_ref.has_value()) {
                auto& prefix = prefix_ref->get();
                for (auto& t : prefix.value()) {
                    output_text(t.string());
                }
            }

            const int bytes = resource_width / kEmbedElementWidth;
            unique_ptr<char[]> buf(new char[bytes]);
            resource_file.read(buf.get(), bytes);
            if (resource_file.gcount() != bytes) {
                error(peek(1), kEmbedResoruceReadingFailureError);
                return EmbedResult::kErrorOrUnsupportedParameter;
            }

            char dec[6] = ", XXX";
            sprintf_s(dec, "%d", (static_cast<unsigned char>(buf[0])));
            output_text(dec);
            for (int i = 1; i < bytes; ++i) {
                if ((i % 16) == 0) {
                    sprintf_s(dec, ",\n%d", (static_cast<unsigned char>(buf[i])));
                } else {
                    sprintf_s(dec, ", %d", (static_cast<unsigned char>(buf[i])));
                }
                output_text(dec);
            }

            auto suffix_ref = spec.parameter(EmbedParameter::kIdentSuffix);
            if (suffix_ref.has_value()) {
                auto& suffix = suffix_ref->get();
                for (auto& t : suffix.value()) {
                    output_text(t.string());
                }
            }
        }
    }

    return (resource_width == 0) ? EmbedResult::kResourceIsEmpty : EmbedResult::kComplete;
}

bool Preprocessor::execute_define() {
    skip_ws();

    Token name = peek(1);
    if (name.type() != TokenType::kIdentifier) {
        error(name, kInvalidMacroName);
        skip_directive_line();
    } else {
        match(TokenType::kIdentifier);

        //  NOTE: マクロ名の直後に空白やコメントを挟まずに括弧が
        //   有る場合のみ、関数型マクロとして解釈する。なので、ここでは
        //   空白を読み飛ばしてはならない。
        bool func_type = (peek(1).type() == TokenType::kLeftParenthesis);
        if (!func_type) {
            optional<TokenList> replist = replacement_list(MacroForm::kObjectLike, Macro::kNoParams);
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

                optional<TokenList> replist = replacement_list(MacroForm::kFunctionLike, *params);
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
    if (name.type() != TokenType::kIdentifier) {
        skip_directive_line();
        new_line();

        error(name, kMustBeMacroName);
        return false;
    }

    match(TokenType::kIdentifier);
    skip_ws();
    TokenList extra_tokens;
    skip_directive_line(&extra_tokens);
    new_line();

    if (!extra_tokens.empty()) {
        error(extra_tokens.front(), kRedundantTokens);
        return false;
    }

    remove_macro(name);

    return true;
}

bool Preprocessor::execute_line(const std::string& line, const std::optional<std::string>& path) {
    if (!is_digit_sequence(line) || (line != "0" && line.starts_with("0"))) {
        error(peek(1), kLineNeedsDecimalConstantError);
        return false;
    }

    constexpr uint32_t kMinLine = 1;
    constexpr uint32_t kMaxLine = 2147483647;

    const auto s = strip_digit_separator(line);
    uint32_t l = 0;
    const auto r = from_chars(&s[0], &s[s.length()], l);
    if (r.ec != errc{} || r.ptr != &s[s.length()]) {
        if (r.ec == errc::result_out_of_range) {
            error(peek(1), kLineOutOfRangeError, kMinLine, kMaxLine);
        } else {
            error(peek(1), as_internal(__func__));
        }
        return false;
    }

    if (!(kMinLine <= l && l <= kMaxLine)) {
        error(peek(1), kLineOutOfRangeError, kMinLine, kMaxLine);
        return false;
    }

    current_source_line_number(l);

    if (path) {
        current_source_path(internal_from_source(unquote_string(path.value())));
    }

    return true;
}

bool Preprocessor::execute_pragma(const TokenList& tokens, const Token& location) {
    if (tokens.empty()) {
        //  空のプラグマは何もしない。
        return true;
    //} else if (tokens[0].string() == "STDC") {
    //    "FP_CONTRACT"
    //    "FENV_ACCESS"
    //    "FENV_DEC_ROUND"
    //    "FENV_ROUND"
    //    "CX_LIMITED_RANGE"
    //    "ON" "OFF" "DEFAULT"
    //    "FE_DOWNWARD", "FE_TONEAREST", "FE_TONEARESTFROMZERO",
    //    "FE_TOWARDZERO", "FE_UPWARD", "FE_DYNAMIC",
    //    "FE_DEC_DOWNWARD", "FE_DEC_TONEAREST", "FE_DEC_TONEARESTFROMZERO",
    //    "FE_DEC_TOWARDZERO", "FE_DEC_UPWARD", "FE_DEC_DYNAMIC",
    } else {
        info(location, kPragmaIsIgnoredWarning, Token::concat_string(tokens));
        return false;
    }
}

void Preprocessor::output_text(std::string_view text) {
    output_->write(text.data(), text.length());
#if !defined(NDEBUG)
    output_->flush();
#endif
}

void Preprocessor::output_text(const char* text) {
    if (!text) {
        output_->write("(NUL)", 5);
        return;
    }

    output_->write(text, strlen(text));
#if !defined(NDEBUG)
    output_->flush();
#endif
}

SourceFile& Preprocessor::current_source() {
    return sources_.current_source();
}

SourceFile* Preprocessor::current_source_pointer() {
    return sources_.current_source_pointer();
}

String Preprocessor::current_source_path() {
    return sources_.current_source_path();
}

void Preprocessor::current_source_path(const String& value) {
    sources_.current_source_path(value);
}

std::uint32_t Preprocessor::current_source_line_number() {
    return sources_.current_source_line_number();
}

void Preprocessor::current_source_line_number(std::uint32_t value) {
    SourceFile& source = current_source();
    source.reset_line_number(value);

    // XXX: 従来の処理が Sourceに対するものであって、TokenStreamに対するものではなかったので、不格好な
    //      形での暫定対応。恐らく、TokenStreamに対して行うようにしても問題は無い気がする。
    auto it = find_if(stream_stack_.rbegin(), stream_stack_.rend(),
            [&source](TokenStream& stream) {
                return stream.input_source() == &source;
            });
    if (it == stream_stack_.rend()) {
        throw runtime_error(__func__);
    }
    (*it).get().reset_line_number(value);
}

void Preprocessor::add_predefined_macro(const std::string& name, const std::string& value, const TokenType type) {
    macros_.insert({ name, Macro::create_macro(name, value, type) });
    predef_macro_names_.push_back(name);
}

bool Preprocessor::is_valid_macro_name(const Token& name) {
    if (is_predefined_macro_name(name.string())) {
        error(name, kPredefinedMacroNameError, name.string());
        return false;
    }
    if (name.string() == kIdentPragma) {
        error(name, kOpPragmaUsedAsMacroName);
        return false;
    }
    if (name.string() == kIdentVaArgs) {
        error(name, kVaArgsIdentifierUsageError);
        return false;
    }
    if (name.string() == kIdentVaOpt) {
        error(name, kVaOptIdentifierUsageError);
        return false;
    }

    if (name.string().find("__STDC_") == 0) {
        if (!current_source().is_system_file()) {
            warning(name, kStdcReservedMacroName);
        }
    }

    return true;
}

MacroPtr Preprocessor::add_macro(const Token& name, const TokenList& replist) {
    if (!is_valid_macro_name(name)) {
        return nullptr;
    }

    auto it = macros_.find(name.string());
    if (it != macros_.end()) {
        MacroPtr m = it->second;
        if (m->is_function() || !token_list_equal(m->replist(), replist)) {
            warning(name, kMacroRedefinitionWarning, name.string(), m->source(), m->line(), m->column());
        }
        m->reset(replist, source_from_internal(current_source_path()), name);
        DEBUG(name, T_("[REDEF] {}"), macro_def_string(MacroForm::kObjectLike, name, Macro::kNoParams, replist));

        return m;
    } else {
        auto [new_it, inserted] = macros_.insert(
                { name.string(), Macro::create_macro(name.string(), replist, source_from_internal(current_source_path()), name) });
        if (!inserted) {
            fatal_error(name, as_internal(__func__) /* ロジックエラーかメモリーが足りないか？ */);
        }
        DEBUG(name, T_("[DEF] {}"), macro_def_string(MacroForm::kObjectLike, name, Macro::kNoParams, replist));

        return new_it->second;
    }
}

MacroPtr Preprocessor::add_macro(const Token& name, const Macro::ParamList& params, const TokenList& replist) {
    if (!is_valid_macro_name(name)) {
        return nullptr;
    }

    auto it = macros_.find(name.string());
    if (it != macros_.end()) {
        MacroPtr m = it->second;
        if (m->is_object() || (m->params() != params) || !token_list_equal(m->replist(), replist)) {
            warning(name, kMacroRedefinitionWarning, name.string(), m->source(), m->line(), m->column());
        }
        m->reset(params, replist, source_from_internal(current_source_path()), name);
        DEBUG(name, T_("[REDEF] {}"), macro_def_string(MacroForm::kFunctionLike, name, params, replist));

        return m;
    } else {
        auto [new_it, inserted] = macros_.insert(
                { name.string(), Macro::create_macro(name.string(), params, replist, source_from_internal(current_source_path()), name) });
        if (!inserted) {
            fatal_error(name, as_internal(__func__) /* ロジックエラー */);
        }
        DEBUG(name, T_("[DEF] {}"), macro_def_string(MacroForm::kFunctionLike, name, params, replist));

        return new_it->second;
    }
}

std::string Preprocessor::macro_def_string(MacroForm form, const Token& name, const Macro::ParamList& params, const TokenList& replist) {
    std::string buf;
    buf.reserve(name.string().length() + 1 + params.size() * 16 + 2 + replist.size() * 32);

    buf += name.string();
    if (form == MacroForm::kFunctionLike) {
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
        warning(name, kUndefineNondefinedMacroWarning, name.string());
    } else {
        macros_.erase(it);
        DEBUG(name, T_("[UNDEF] {}"), name.string());
    }
}

MacroPtr Preprocessor::find_macro(const std::string& name) {
    auto it = macros_.find(name);
    if (it == macros_.end()) {
        return nullptr;
    }
    MacroPtr m = it->second;
    if (name == "__FILE__") {
        m->reset({ Token(quote_string(source_string(current_source_path())), TokenType::kStringLiteral) }, "", kTokenNull);
    } else if (name == "__LINE__") {
        m->reset({ Token(to_string(current_source_line_number()), TokenType::kPpNumber)}, "", kTokenNull);
    }

    return m;
}

void Preprocessor::print_macros() {
    for (const auto& kv : macros_) {
        auto& m = kv.second;
        log_debug(T_("{}"), m->name());
        if (m->is_function()) {
            log_debug(T_("("));
            if (!m->params().empty()) {
                auto it = m->params().begin();
                log_debug(T_("{}"), (*it));
                ++it;
                for (; it != m->params().end(); ++it) {
                    log_debug(T_(", {}"), (*it));
                }
            }
            log_debug(T_(")"));
        }
        log_debug(T_(": "));

        for (const auto& r : m->replist()) {
            log_debug(T_("{}"), r.string());
        }

        log_debug(T_("\n"));
    }
}


void init_preprocessor() {
    sort(operators.begin(), operators.end(), OperatorLess());
    sort(directives.begin(), directives.end(),
            [](const auto& lhs, const auto& rhs) { return get<0>(lhs) < get<0>(rhs); });
}

}   //  namespace pp
