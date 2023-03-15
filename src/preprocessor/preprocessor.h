#ifndef CC_PREPROCESSOR_PREPROCESSOR_H_
#define CC_PREPROCESSOR_PREPROCESSOR_H_

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <stack>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "preprocessor/pp_config.h"
#include "preprocessor/calculator.h"
#include "preprocessor/diagnostics.h"
#include "preprocessor/input.h"
#include "preprocessor/options.h"
#include "preprocessor/scanner.h"
#include "util/utility.h"

#if defined(NDEBUG)
#define DEBUG(t, ...)
#define DEBUG_EXPR(expr)
#else
#define DEBUG(t, ...)       debug(t, __VA_ARGS__)
#define DEBUG_EXPR(expr)    expr
#endif

namespace pp {

extern const StringView kNoInputError;
extern const StringView kNoSuchFileError;
extern const StringView kFileOutputError;
extern const StringView kErrorFileOutputError;

/**
 */
enum class MacroForm {
    kObjectLike,
    kFunctionLike,
};

/**
 */
enum class MacroExpantionMethod {
    kDirectlyCopyable,
    kNormal,
    kOpPragma,
    kVaOpt,
    kOpHasCAttribute,
    kOpHasInclude,
    kOpHasEmbed,

    kNumElements,
};

constexpr auto kNumOfMacroExpantionMethod = lib::util::enum_ordinal(MacroExpantionMethod::kNumElements);

class Macro;
using MacroPtr = std::shared_ptr<Macro>;
using MacroSet = std::unordered_map<std::string, MacroPtr>;

/**
 */
class Macro {
public:
    using ParamList = std::vector<std::string>;
    using ArgList = std::vector<TokenList>;

    static MacroPtr create_macro(const std::string& name, const TokenList& replist, const std::string& source, const Token& name_token);
    static MacroPtr create_macro(const std::string& name, const Macro::ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token);
    static MacroPtr create_macro(const std::string& name, const std::string& value, const TokenType type);

    static inline const ParamList kNoParams = {};
    static inline const ArgList kNoArgs = {};

    template<typename Base>
    struct CreateHelper : Base {
        template<typename... Args>
        explicit CreateHelper(Args&&... args) : Base(std::forward<Args>(args)...) {
        }
    };

    Macro(const Macro&) = delete;
    Macro(Macro&&) = delete;
    ~Macro();

    Macro& operator=(const Macro&) = delete;
    Macro& operator=(Macro&&) = delete;

    const std::string& name() const { return name_; }
    MacroForm form() const { return form_; }
    MacroExpantionMethod expantion_method() const { return expantion_method_; }
    const ParamList& params() const { return params_; }
    const TokenList& replist() const { return replist_; }
    bool has_va_args() const { return has_va_args_; }
    bool is_function() const { return form() == MacroForm::kFunctionLike; }
    bool is_object() const { return form() == MacroForm::kObjectLike; }
    bool is_predefined() const { return is_predefined_; }
    //bool needs_concat() const { return needs_concat_; }
    //bool needs_stringize() const { return needs_stringize_; }
    const std::string& source() const { return source_; }
    std::uint32_t line() const { return line_; }
    std::uint32_t column() const { return column_; }

    std::size_t param_index_of(const std::string& param_name) const;
    void reset(const TokenList& replist, const std::string& source, const Token& name_token);
    void reset(const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token);

private:
    Macro(const std::string& name, const TokenList& replist, const std::string& source, const Token& name_token);
    Macro(const std::string& name, const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token);
    Macro(const std::string& name, const std::string& value, const TokenType type);

    MacroExpantionMethod get_expantion_method(MacroForm form, const std::string& name, const TokenList& replist);

    std::string name_;
    MacroForm form_;
    MacroExpantionMethod expantion_method_;
    ParamList params_;
    TokenList replist_;
    bool has_va_args_;
    bool is_predefined_;
    //bool needs_concat_;
    //bool needs_stringize_;
    std::string source_;
    std::uint32_t line_;
    std::uint32_t column_;
};

/**
 */
class IncludeSpec {
public:
    explicit IncludeSpec(const std::string& header_name);
    ~IncludeSpec() = default;

    std::string header_name() const;
    bool is_includes_source_dir() const;

private:
    std::string header_name_;
};

/**
 */
class EmbedParameter {
public:
    static constexpr inline std::string_view kIdentLimit = "limit";
    static constexpr inline std::string_view kIdentPrefix = "prefix";
    static constexpr inline std::string_view kIdentSuffix = "suffix";
    static constexpr inline std::string_view kIdentIfEmpty = "if_empty";

    static bool is_standard_parameter_name(std::string_view name) {
        return standard_embed_parameters_.find(name) != standard_embed_parameters_.end();
    }

    static bool is_supported_parameter_name(const std::string& name) {
        return supported_embed_parameters_.find(name) != supported_embed_parameters_.end();
    }

    explicit EmbedParameter(const std::string& name)
        : name_(name)
        , value_()
        , has_value_(false) {

    }

    EmbedParameter(EmbedParameter&&) = default;

    ~EmbedParameter() {
    }


    EmbedParameter& operator=(EmbedParameter&&) = default;

    const std::string& name() const {
        return name_;
    }

    const TokenList& value() const {
        return value_;
    }

    void set_value(TokenList&& tokens) {
        value_ = std::move(tokens);
        has_value_ = true;
    }

    bool has_value() const {
        return has_value_;
    }

private:
    static const inline std::set<std::string_view> standard_embed_parameters_ = {
        kIdentLimit, kIdentPrefix, kIdentSuffix, kIdentIfEmpty,
    };

    static const inline std::set<std::string_view> supported_embed_parameters_ = {
        kIdentLimit, kIdentPrefix, kIdentSuffix, kIdentIfEmpty,
    };

    std::string name_;
    TokenList value_;
    bool has_value_;
};

inline
bool operator<(const EmbedParameter& lhs, std::string_view rhs) {
    return lhs.name() < rhs;
}

inline
bool operator<(std::string_view lhs, const EmbedParameter& rhs) {
    return lhs < rhs.name();
}

inline
bool operator<(const EmbedParameter& lhs, const EmbedParameter& rhs) {
    return lhs.name() < rhs.name();
}

/**
 */
class EmbedSpec {
public:
    using ParameterList = std::set<EmbedParameter, std::less<>>;

    explicit EmbedSpec(const std::string& resource_id)
        : resource_id_(resource_id) {
    }

    ~EmbedSpec() = default;

    const std::string& resource_id() const {
        return resource_id_;
    }

    std::optional<std::reference_wrapper<const EmbedParameter>> parameter(std::string_view name) const {
        auto it = parameters_.find(name);
        if (it != parameters_.end()) {
            return *it;
        } else {
            return std::nullopt;
        }
    }

    void add_parameter(EmbedParameter&& param) {
        // XXX: 同じ名前のパラメーターが複数指定できる場合について考えていない。
        parameters_.insert(std::move(param));
    }

    const ParameterList& parameters() const {
        return parameters_;
    }

private:
    std::string resource_id_;
    ParameterList parameters_;
};

/**
 */
enum class EmbedResult {
    kErrorOrUnsupportedParameter = 0,
    kComplete = 1,
    kResourceIsEmpty = 2,   // limit(0)も含む。
};


/**
 */
class Preprocessor {
public:
    explicit Preprocessor(const Options& opts, Diagnostics& diag);
    ~Preprocessor();

    bool has_error();
    int run();

private:
    bool cleanup();
    void prepare_predefined_macro();
    void preprocessing_file(std::istream* input, const String& path);
    void group(SourceFile& source, Group& group);
    bool group_part();
    void if_section();
    TokenList make_constant_expression();
    target_intmax_t constant_expression(const TokenList& expr_tokens, const Token& dir_token);
    void calc(std::stack<Operator>& ops, std::stack<target_intmax_t>& nums, const Operator& next_op);
    bool if_group();
    bool elif_groups(bool processed);
    bool elif_group(bool processed);
    void else_group(bool processed);
    void endif_line();
    void control_line(TokenType directive);
    void text_line(const TokenList& ws_tokens);

    const TokenList& get_expanded_arg(std::size_t n, const TokenList& arg, Macro::ArgList& cache);

#if !defined(NDEBUG)
    class Indent {
    public:
        static inline int indent_level_ = 0;

        explicit Indent(bool inc = true)
            : inc_(inc)
        {
            if (inc_) {
                ++indent_level_;
            }
        }
        ~Indent() {
            if (inc_) {
                --indent_level_;
            }
        }

        static std::string tab() {
            return std::string(Indent::indent_level_ * 1, ' ');
        }

    private:
        bool inc_;
    };
#endif

    bool expand(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_directly_copyable(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_normal(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_op_pragma(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_va_opt(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_op_has_c_attribute(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_op_has_include(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
    bool expand_op_has_embed(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);

    TokenList expand_directive_line();

    bool search_include_file(const IncludeSpec& include_spec, pp::String* file_path_str);

    using MacroExpantionFuncPtr = bool (Preprocessor::*)(const Macro&, const Macro::ArgList&, TokenList&);
    static constexpr MacroExpantionFuncPtr expantion_methods_[kNumOfMacroExpantionMethod] = {
        &Preprocessor::expand_directly_copyable,
        &Preprocessor::expand_normal,
        &Preprocessor::expand_op_pragma,
        &Preprocessor::expand_va_opt,
        &Preprocessor::expand_op_has_c_attribute,
        &Preprocessor::expand_op_has_include,
        &Preprocessor::expand_op_has_embed,
    };

    TokenList substitute_by_arg_if_need(const Macro& macro, const Macro::ArgList& macro_args, const Token& token);
    void scan(TokenList& result_expanded);

    void non_directive();
    //void lparen();
    std::optional<TokenList> replacement_list(MacroForm macro_form, const Macro::ParamList& macro_params);
    std::optional<Macro::ParamList> read_macro_params();
    template <class Funcion>
    Token read_macro_arg_loop(TokenList* read_tokens, TokenList& result, Funcion end_condition);
    Token read_macro_arg(TokenList* read_tokens, TokenList& result);
    Token read_macro_arg_at_ellipsis(TokenList* read_tokens, TokenList& result);
    std::optional<Macro::ArgList> read_macro_args(const Macro& macro, TokenList* read_tokens);

    //void pp_tokens(bool output);

    void new_line();

    bool pp_balanced_token(TokenList* tokens);
    bool pp_balanced_token_sequence(TokenType left_bracket, TokenList* result_tokens);
    bool pp_parameter_name(std::string* result_name);
    bool pp_paramter_clause(EmbedParameter* parameter);
    bool pp_parameter(EmbedSpec* spec);
    bool embed_parameter_sequence(EmbedSpec* spec);

    void skip_directive_line(TokenList* skipped_tokens = nullptr);
    void skip_ws(TokenList* read_tokens = nullptr);
    void skip_ws_and_nl(TokenList* read_tokens, bool* broken);

    void match(TokenType type);
    void match(const std::string& seq);
    void match(const char* seq);

    void consume() {
        assert(!stream_stack_.empty());

        TokenStream& s = stream_stack_.back();
        s.consume();
    }

    const Token& peek(int i) {
        assert(!stream_stack_.empty());

        TokenStream& s = stream_stack_.back();
        return s.peek(i);
    }

    void push_stream(TokenStream& stream) {
        stream_stack_.push_back(stream);
    }

    void pop_stream() {
        assert(!stream_stack_.empty());

        stream_stack_.pop_back();
    }

    void replace_stream(TokenList&& tokens) {
        assert(!stream_stack_.empty());

        TokenStream& s = stream_stack_.back();
        s.insert(move(tokens));
    }

    std::string execute_stringize(const Macro::ArgList& args, Macro::ArgList::size_type first, Macro::ArgList::size_type last);
    bool execute_include(const std::string& header_name, const Token& header_name_token);
    EmbedResult execute_embed(EmbedSpec& spec, bool has_embed_context);
    bool execute_define();
    bool execute_undef();
    bool execute_line(const std::string& line, const std::optional<std::string>& path);
    bool execute_pragma(const TokenList& tokens, const Token& location);

    void output_text(const StringView& text);
    void output_text(const char* text);

    template <class... Ts>
    void debug(const Token& token, const StringView& format, const Ts&... args) {
        if (diag_level_ <= DiagLevel::kDebug) {
            diag_.debug(current_source_pointer(), token, format, args...);
        }
    }

    template <class... Ts>
    void info(const Token& token, const StringView& format, const Ts&... args) {
        if (diag_level_ <= DiagLevel::kInfo) {
            diag_.info(current_source_pointer(), token, format, args...);
        }
    }

    template <class... Ts>
    void warning(const Token& token, const StringView& format, const Ts&... args) {
        diag_.warning(current_source_pointer(), token, format, args...);
    }

    template <class... Ts>
    void error(const Token& token, const StringView& format, const Ts&... args) {
        diag_.error(current_source_pointer(), token, format, args...);
    }

    template <class... Ts>
    [[noreturn]]
    void fatal_error(const Token& token, const StringView& format, const Ts&... args) {
        diag_.fatal_error(current_source_pointer(), token, format, args...);
    }

    SourceFile& current_source();
    SourceFile* current_source_pointer();
    String current_source_path();
    void current_source_path(const String& value);
    std::uint32_t current_source_line_number();
    void current_source_line_number(std::uint32_t value);

    void add_predefined_macro(const std::string& name, const std::string& value, const TokenType type);
    bool is_predefined_macro_name(const std::string& name) {
        auto it = std::lower_bound(predef_macro_names_.begin(), predef_macro_names_.end(), name);
        return (it != predef_macro_names_.end()) && !(name < *it);
    }
    bool is_valid_macro_name(const Token& name);
    MacroPtr add_macro(const Token& name, const TokenList& replist);
    MacroPtr add_macro(const Token& name, const Macro::ParamList& params, const TokenList& replist);
    std::string macro_def_string(MacroForm form, const Token& name, const Macro::ParamList& params, const TokenList& replist);
    void remove_macro(const Token& name);
    MacroPtr find_macro(const std::string& name);
    void print_macros();

    const Options& opts_;
    Diagnostics& diag_;

    std::vector<String> include_dirs_;
    DiagLevel diag_level_;
    clock_t clock_start_;
    clock_t clock_end_;
    std::vector<std::reference_wrapper<TokenStream>> stream_stack_;

    std::ostream* output_;
    std::vector<char> output_buffer_;
    std::ofstream output_file_;
    std::ostream* error_output_;
    std::vector<char> error_output_buffer_;
    std::shared_ptr<std::ofstream> error_file_;
    std::stack<SourceFile*> sources_;
    MacroSet macros_;
    std::vector<std::string> predef_macro_names_;
    std::unordered_set<std::string> used_macro_names_;
    Token error_location_;

    int included_files_;
    int rescan_count_;

    struct MacroInvocation {
        const Macro* macro;
        const Macro::ArgList* args;
        Macro::ArgList* expanded_args;
    };
    std::vector<MacroInvocation> macro_invocation_stack_;
};

/**
 */
void init_preprocessor();

}   //  namespace pp

#endif  // CC_PREPROCESSOR_PREPROCESSOR_H_
