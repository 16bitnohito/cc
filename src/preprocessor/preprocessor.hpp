#if !defined(PREPROCESSOR_PREPROCESSOR_HPP_)
#define PREPROCESSOR_PREPROCESSOR_HPP_

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <ctime>
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
#include <preprocessor/calculator.hpp>
#include <preprocessor/config.hpp>
#include <preprocessor/diagnostics.hpp>
#include <preprocessor/scanner.hpp>
#include <preprocessor/utility.hpp>


namespace pp {

constexpr char kUnknownOptionError[] = "不明なオプション %cが指定された。\n";
constexpr char kNoOptionParameterError[] = "オプション %cの値が指定されていない。\n";
constexpr char kNoInputError[] = "入力ファイルが指定されていない。\n";
constexpr char kNoSuchFileError[] = "ファイルが開けない: %s\n";
constexpr char kFileOutputError[] = "出力に失敗した。\n";

extern const Token kTokenEndOfFile;

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

	kNumElements,
};

constexpr auto kNumOfMacroExpantionMethod = enum_ordinal(MacroExpantionMethod::kNumElements);

/**
 */
struct Macro {
public:
	using ParamList = std::vector<std::string>;
	using ArgList = std::vector<TokenList>;

	static inline const ParamList kNoParams = {};
	static inline const ArgList kNoArgs = {};

	Macro(const std::string& name, const TokenList& replist, const std::string& source, const Token& name_token);
	Macro(const std::string& name, const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token);
	Macro(const std::string& name, const std::string& value, const TokenType type);
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
	uint32_t line() const { return line_; }
	uint32_t column() const { return column_; }

	std::size_t param_index_of(const std::string& param_name) const;
	void reset(const TokenList& replist, const std::string& source, const Token& name_token);
	void reset(const ParamList& params, const TokenList& replist, const std::string& source, const Token& name_token);

private:
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
	uint32_t line_;
	uint32_t column_;
};

using MacroPtr = std::shared_ptr<Macro>;
using MacroSet = std::unordered_map<std::string, MacroPtr>;


/**
 */
class Preprocessor {
public:
	class Options {
	public:
		Options();
		~Options();

		const std::string& input_filepath() const;
		const std::string& output_filepath() const;
		const std::string& error_log_filepath() const;
		bool output_line_directive() const;
		bool output_comment() const;
		bool support_trigraphs() const;
		const std::vector<std::string>& additional_include_dirs() const;
		const std::vector<std::string>& macro_defs() const;
		const std::vector<std::string>& macro_undefs() const;

		bool parse_options(const std::vector<std::string>& args);
		void print_usage();

	private:
		std::string input_encoding_;
		std::string input_filepath_;
		std::string output_encoding_;
		std::string output_filepath_;
		std::string error_log_filepath_;
		bool output_line_directive_;
		bool output_comment_;
		bool support_trigraphs_;
		std::vector<std::string> additional_include_dirs_;
		std::vector<std::string> macro_defs_;
		std::vector<std::string> macro_undefs_;
	};

	Preprocessor();
	~Preprocessor();

	bool has_error();
	bool parse_options(const std::vector<std::string>& args);
	void print_usage();
	int run();

private:
	struct Group {
		bool processing;
		uint32_t start_line_number;
		TokenType type;

		Group(bool p, uint32_t l, TokenType t) {
			processing = p;
			start_line_number = l;
			type = t;
		}
		~Group() {
		}
	};


	struct Source {
		std::string path;
		uint32_t line;
		Preprocessor* pp;
		Scanner* scanner;
		int p;
		std::vector<Token> lookahead;
		int condition_level;
		std::stack<Group*> groups;

		TokenList queue;
		TokenList::size_type q;

		Source(const std::string& filepath, Scanner* src_scanner, Preprocessor* preprocessor);
		Source(const Source&) = delete;
		Source(Source&&) = delete;
		~Source();

		Source& operator=(const Source&) = delete;
		Source& operator=(Source&&) = delete;

		std::string parent_dir();

		void consume() {
			if (q < queue.size()) {
				q++;
			} else {
				lookahead[p] = scanner->next_token();
				p = (p + 1) % kNumLookahead;

				const Token& t = lookahead[p];
				if (line != t.line()) {
					line = t.line();
				}
			}
		}

		void insert(TokenList&& tokens) {
			queue = move(tokens);
			q = 0;
		}

		const Token& peek(int i) {
			if (q < queue.size()) {
				// XXX: q + (i - 1) >= queue.size()の場合
				return queue[q + (i - 1)];
			} else {
				return lookahead[(p + i - 1) % kNumLookahead];
			}
		}

		void reset_line_number(uint32_t new_line_number);
	};


	class TokenStream {
	public:
		class Buffer {
		public:
			virtual ~Buffer() {}
			virtual void consume() = 0;
			virtual const Token& peek(int i) const = 0;
		};

		explicit TokenStream(const TokenList& tokens)
			: buffer_(std::make_unique<TokenStreamFromTokenList>(tokens))
			, queue_()
			, q_() {
		}

		explicit TokenStream(const std::string& string, Preprocessor::Options& opts)
			: buffer_(std::make_unique<TokenStreamFromString>(string, opts))
			, queue_()
			, q_() {
		}

		~TokenStream() {
		}

		void consume() {
			if (q_ < queue_.size()) {
				++q_;
			} else {
				buffer_->consume();
			}
		}

		void insert(TokenList&& tokens) {
			queue_ = move(tokens);
			q_ = 0;
		}

		const Token& peek(int i) const {
			if (q_ < queue_.size()) {
				// XXX: q_ + (i - 1) >= queue_.size()の場合
				return queue_[q_ + (i - 1)];
			} else {
				return buffer_->peek(i);
			}
		}

	private:
		std::unique_ptr<Buffer> buffer_;
		TokenList queue_;
		TokenList::size_type q_;
	};

	using TokenStreamPtr = std::shared_ptr<TokenStream>;


	class TokenStreamFromTokenList
		: public TokenStream::Buffer {
	public:
		explicit TokenStreamFromTokenList(const TokenList& tokens)
			: tokens_ref_(tokens)
			, i_() {
		}

		virtual ~TokenStreamFromTokenList() {
		}

		virtual void consume() override {
			++i_;
		}

		virtual const Token& peek(int i) const override {
			auto x = i_ + (i - 1);
			if (x >= tokens_ref_.size()) {
				return kTokenEndOfFile;
			} else {
				return tokens_ref_[x];
			}
		}

	private:
		const TokenList& tokens_ref_;
		uint32_t i_;
	};


	class TokenStreamFromString
		: public TokenStream::Buffer {
	public:
		explicit TokenStreamFromString(const std::string& string, Preprocessor::Options& opts)
			: in_(string)
			, scanner_(&in_, opts.support_trigraphs(), false)
			, lookahead_(scanner_.next_token()) {
		}

		virtual ~TokenStreamFromString() {
		}

		void consume() override {
			lookahead_ = scanner_.next_token();
		}

		const Token& peek(int /*i*/) const override {
			return lookahead_;
		}

	private:
		std::istringstream in_;
		Scanner scanner_;
		//  XXX: 現状 1つしか先読みしないので、Sourceでやっているのと同じではない。別の方法にした方が良い。
		Token lookahead_;
	};


	class GroupScope {
	public:
		GroupScope(Source* source, Group* group)
			: source_(source) {
			source_->groups.push(group);
		}
		~GroupScope() {
			source_->groups.pop();
		}

	private:
		Source* source_;
	};


	bool cleanup();
	void prepare_predefined_macro();
	void preprocessing_file(std::istream* input, const std::string& path);
	void group(Source& source, Group& group);
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

	bool expand(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded) {
#if !defined(NDEBUG)
		Indent indent;
#endif

		auto ord = enum_ordinal(macro.expantion_method());
		return (this->*expantion_methods_[ord])(macro, macro_args, result_expanded);
	}
	bool expand_directly_copyable(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
	bool expand_normal(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);
	bool expand_op_pragma(const Macro& macro, const Macro::ArgList& macro_args, TokenList& result_expanded);

	using MacroExpantionFuncPtr = bool (Preprocessor::*)(const Macro&, const Macro::ArgList&, TokenList&);
	static constexpr MacroExpantionFuncPtr expantion_methods_[kNumOfMacroExpantionMethod] = {
		&Preprocessor::expand_directly_copyable,
		&Preprocessor::expand_normal,
		&Preprocessor::expand_op_pragma,
	};

	TokenList substitute_by_arg_if_need(const Macro& macro, const Macro::ArgList& macro_args, const Token& token);
	void scan(TokenList& result_expanded);

	void non_directive();
	//void lparen();
	std::optional<TokenList> replacement_list(MacroForm macro_form, const Macro::ParamList& macro_params);
	std::optional<Macro::ParamList> read_macro_params();
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
		if (!stream_stack_.empty()) {
			auto ts = stream_stack_.back();
			ts->consume();
		} else {
			Source& src = *sources_.top();
			src.consume();
		}
	}

	const Token& peek(int i) {
		if (!stream_stack_.empty()) {
			TokenStream& str = *stream_stack_.back();
			return str.peek(i);
		} else {
			Source& src = *sources_.top();
			return src.peek(i);
		}
	}

	void push_stream(const TokenList& tokens) {
		auto s = std::make_shared<TokenStream>(tokens);
		stream_stack_.push_back(s);
	}

	void push_stream(const std::string& string) {
		auto s = std::make_shared<TokenStream>(string, opts_);
		stream_stack_.push_back(s);
	}

	void pop_stream() {
		stream_stack_.pop_back();
	}

	void replace_stream(TokenList&& tokens) {
		if (!stream_stack_.empty()) {
			auto s = stream_stack_.back();
			s->insert(move(tokens));
		} else {
			Source& src = *sources_.top();
			src.insert(move(tokens));
		}
	}

	std::string execute_stringize(const Macro::ArgList& args, Macro::ArgList::size_type first, Macro::ArgList::size_type last);
	bool execute_include(const std::string& header_name, const Token& header_name_token);
	bool execute_define();
	bool execute_undef();
	bool execute_pragma(const TokenList& tokens, const Token& location);

	void output_text(const std::string& text) {
		output_text(text.c_str());
	}
	void output_text(const char* text);
	void output_error(const char* format, ...);
	void output_log(DiagLevel level, const Token& token, const char* message, va_list args);
	void debug(const Token& token, const char* format, ...);
	void debug(const Token& token, const std::string& message);
	void info(const Token& token, const char* format, ...);
	void info(const Token& token, const std::string& message);
	void warning(const Token& token, const char* format, ...);
	void warning(const Token& token, const std::string& message);
	void error(const Token& token, const char* format, ...);
	void error(const Token& token, const std::string& message);
	[[noreturn]] void fatal_error(const Token& token, const char* format, ...);
	[[noreturn]] void fatal_error(const Token& token, const std::string& message);

	std::string current_source_path();
	void current_source_path(const std::string& value);
	uint32_t current_source_line_number();
	void current_source_line_number(uint32_t value);

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

	static constexpr int kNumLookahead = 2;

	Options opts_;

	std::vector<std::string> include_dirs_;
	DiagLevel diag_level_;
	clock_t clock_start_;
	clock_t clock_end_;
	std::vector<TokenStreamPtr> stream_stack_;

	FILE* output_;
	//std::vector<char> output_buf_;
	FILE* error_output_;
	std::stack<Source*> sources_;
	MacroSet macros_;
	std::vector<std::string> predef_macro_names_;
	std::unordered_set<std::string> used_macro_names_;
	Token error_location_;

	int32_t warning_count_;
	int32_t error_count_;

	int included_files_;
	int rescan_count_;
};

}   //  namespace pp

#endif  //  PREPROCESSOR_PREPROCESSOR_HPP_
