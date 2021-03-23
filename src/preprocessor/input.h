#ifndef CC_PREPROCESSOR_INPUT_H_
#define CC_PREPROCESSOR_INPUT_H_

#include <stack>
#include <preprocessor/options.h>
#include <preprocessor/scanner.h>
#include <preprocessor/token.h>

namespace pp {

extern const std::string kPathDelimiter;

/**
 */
class TokenStream {
public:
    class Buffer {
    public:
        virtual ~Buffer() {}
        virtual void consume() = 0;
        virtual const Token& peek(int i) const = 0;
    };

    explicit TokenStream(const TokenList& tokens);
    explicit TokenStream(const std::string& string, const Options& opts);
    explicit TokenStream(std::istream* input, const Options& opts);
    // explicit TokenStream(FILE* file, const Options& opts);
    ~TokenStream();

    void consume();
    void insert(TokenList&& tokens);
    const Token& peek(int i) const;

private:
    std::unique_ptr<Buffer> buffer_;
    TokenList queue_;
    TokenList::size_type q_;
};

using TokenStreamPtr = std::shared_ptr<TokenStream>;

/**
 */
struct Group {
	bool processing;
	TokenType type;

	Group(bool p, TokenType t) {
		processing = p;
		type = t;
	}

	~Group() {
	}
};

/**
 */
class Source {
public:
	friend class ConditionScope;

	class ConditionScope {
	public:
		explicit ConditionScope(Source* source)
			: source_(source) {
			source_->condition_level_++;
		}

		~ConditionScope() {
			source_->condition_level_--;
		}

	private:
		Source* source_;
	};

	static constexpr int kNumLookahead = 2;

	static Source* create_from_istream(std::istream& input);
	static Source* create_from_string(const std::string& input);
	static Source* create_from_token_list(const TokenList& input);

	TokenStreamPtr get_token_stream() const;

	Source(const std::string& source_file, Scanner* src_scanner);
	Source(const Source&) = delete;
	Source(Source&&) = delete;
	~Source();

	Source& operator=(const Source&) = delete;
	Source& operator=(Source&&) = delete;

	std::string parent_dir();
	uint32_t line() const { return line_; }
	uint32_t column() const { return scanner_->column(); }
	std::size_t num_groups() const { return groups_.size(); }
	Group* current_group() const { return groups_.top(); }

	void consume();
	void insert(TokenList&& tokens);
	const Token& peek(int i);

	void reset_line_number(uint32_t new_line_number);

	void push_group(Group* group);
	void pop_group();

	int condition_level() const { return condition_level_; }
	ConditionScope enter_condition_scope() { return ConditionScope(this); }

	void scanner_hint(ScannerHint hint) { scanner_->state_hint(hint); }

	const std::string& source_path() const { return path_; }
	void source_path(const std::string& value) { path_ = value; }

private:
	std::string path_;
	uint32_t line_;
	Scanner* scanner_;
	std::vector<Token> lookahead_;
	int p_;
	int condition_level_;
	std::stack<Group*> groups_;

	TokenList queue_;
	TokenList::size_type q_;
};

/**
 */
class GroupScope {
public:
	GroupScope(Source* source, Group* group)
		: source_(source) {
		source_->push_group(group);
	}

	~GroupScope() {
		source_->pop_group();
	}

private:
	Source* source_;
};

}   // namespace pp

#endif  // CC_PREPROCESSOR_INPUT_H_
