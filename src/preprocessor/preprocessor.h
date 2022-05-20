#ifndef PREPROCESSOR_PREPROCESSOR_H_
#define PREPROCESSOR_PREPROCESSOR_H_

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <memory>
#include <optional>
#include <stack>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <preprocessor/calculator.h>
#include <preprocessor/config.h>
#include <preprocessor/diagnostics.h>
#include <preprocessor/input.h>
#include <preprocessor/options.h>
#include <preprocessor/scanner.h>
#include <preprocessor/utility.h>


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

    kNumElements,
};

constexpr auto kNumOfMacroExpantionMethod = enum_ordinal(MacroExpantionMethod::kNumElements);

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
class Preprocessor {
public:
    explicit Preprocessor(const Options& opts);
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
    target_intmax_t constant_expression(TokenList&& expr_tokens, const Token& dir_token);
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

    using MacroExpantionFuncPtr = bool (Preprocessor::*)(const Macro&, const Macro::ArgList&, TokenList&);
    static constexpr MacroExpantionFuncPtr expantion_methods_[kNumOfMacroExpantionMethod] = {
        &Preprocessor::expand_directly_copyable,
        &Preprocessor::expand_normal,
        &Preprocessor::expand_op_pragma,
        &Preprocessor::expand_va_opt,
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
    bool execute_define();
    bool execute_undef();
    bool execute_pragma(const TokenList& tokens, const Token& location);

    void output_text(const StringView& text);
    void output_text(const char* text);

    void output_log_with_args(
            DiagLevel level, const Token& token, const StringView& format,
            //const std::format_args_t<ErrorOutputIterator, char>& args);
            const std::format_args& args);

    template <class... Ts>
    void output_log(DiagLevel level, const Token& token, const StringView& format, const Ts&... args) {
        //using Context = std::basic_format_context<ErrorOutputIterator, char>;
        //output_log_with_args(level, token, format, std::make_format_args<Context>(args...));
        output_log_with_args(level, token, format, std::make_format_args(args...));
    }

    template <class... Ts>
    void debug(const Token& token, const StringView& format, const Ts&... args) {
        output_log(DiagLevel::kDebug, token, format, args...);
    }

    template <class... Ts>
    void info(const Token& token, const StringView& format, const Ts&... args) {
        output_log(DiagLevel::kInfo, token, format, args...);
    }

    template <class... Ts>
    void warning(const Token& token, const StringView& format, const Ts&... args) {
        output_log(DiagLevel::kWarning, token, format, args...);
        warning_count_++;
    }

    template <class... Ts>
    void error(const Token& token, const StringView& format, const Ts&... args) {
        output_log(DiagLevel::kError, token, format, args...);
        error_count_++;
    }

    template <class... Ts>
    [[noreturn]] void fatal_error(const Token& token, const StringView& format, const Ts&... args) {
        output_log(DiagLevel::kFatalError, token, format, args...);
        std::exit(EXIT_FAILURE);
    }

    SourceFile& current_source();
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
    const MacroPtr find_macro(const std::string& name);
    void print_macros();

    const Options& opts_;

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

    std::int32_t warning_count_;
    std::int32_t error_count_;

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

#endif  //  PREPROCESSOR_PREPROCESSOR_H_
