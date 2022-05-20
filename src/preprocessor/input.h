#ifndef CC_PREPROCESSOR_INPUT_H_
#define CC_PREPROCESSOR_INPUT_H_

#include <sstream>
#include <stack>
#include <preprocessor/config.h>
#include <preprocessor/options.h>
#include <preprocessor/scanner.h>
#include <preprocessor/token.h>
#include <preprocessor/utility.h>

namespace pp {

extern const String kPathDelimiter;

/**
 */
class Source {
public:
    Source() = default;
    Source(const Source&) = delete;
    virtual ~Source() = default;

    Source& operator=(const Source&) = delete;

    virtual Token next_token() = 0;
};

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
class SourceFile
    : public Source {
public:
    friend class ConditionScope;

    explicit SourceFile(std::istream& input, const String& filepath, const Options& opts);
    virtual ~SourceFile() override;

    virtual Token next_token() override;

    void scanner_hint(ScannerHint hint);

    const String& source_path() const { return path_; }
    void source_path(const String& value) { path_ = value; }

    int condition_level() const { return condition_level_; }
    void inc_condition_level() { ++condition_level_; }
    void dec_condition_level() { --condition_level_; }

    String parent_dir();
    std::size_t num_groups() const { return groups_.size(); }
    Group* current_group() const { return groups_.top(); }

    void reset_line_number(std::uint32_t new_line_number);

    void push_group(Group& group);
    void pop_group();

    std::uint32_t line();
    std::uint32_t column();

private:
    Scanner scanner_;
    String path_;
    int condition_level_;
    std::stack<Group*> groups_;
    std::uint32_t line_;
};

/**
 */
class SourceString
    : public Source {
public:
    explicit SourceString(const std::string& string, const Options& opts);
    virtual ~SourceString() override;

    virtual Token next_token() override;

private:
    std::istringstream in_;
    Scanner scanner_;
};

/**
 */
class SourceTokenList
    : public Source {
public:
    explicit SourceTokenList(const TokenList& tokens);
    virtual ~SourceTokenList() override;

    virtual Token next_token() override;

private:
    std::reference_wrapper<const TokenList> tokens_ref_;
    std::size_t i_;
};

/**
 */
class GroupScope {
public:
    GroupScope(SourceFile& source, Group& group)
        : source_(source) {
        source_.push_group(group);
    }

    GroupScope(const GroupScope&) = delete;
    GroupScope(GroupScope&&) = delete;

    ~GroupScope() {
        source_.pop_group();
    }

    GroupScope& operator=(const GroupScope&) = delete;
    GroupScope& operator=(GroupScope&&) = delete;

private:
    SourceFile& source_;
};

class ConditionScope {
public:
    explicit
    ConditionScope(SourceFile& source)
        : source_(source) {
        source_.inc_condition_level();
    }

    ConditionScope(const ConditionScope&) = delete;
    ConditionScope(ConditionScope&&) = delete;

    ~ConditionScope() {
        source_.dec_condition_level();
    }

    ConditionScope& operator=(const ConditionScope&) = delete;
    ConditionScope& operator=(ConditionScope&&) = delete;

private:
    SourceFile& source_;
};

/**
 */
class TokenStream {
public:
    static constexpr int kNumLookahead = 2;

    explicit TokenStream(Source& input);
    TokenStream(const TokenStream&) = delete;
    ~TokenStream();

    TokenStream& operator=(const TokenStream&) = delete;

    Source* input_source() const;

    void consume();
    void insert(TokenList&& tokens);
    const Token& peek(int i) const;

    void reset_line_number(std::uint32_t new_line_number);

private:
    std::reference_wrapper<Source> input_source_;
    std::vector<Token> lookahead_;
    std::size_t p_;
    TokenList queue_;
    std::size_t q_;
};

using TokenStreamPtr = std::shared_ptr<TokenStream>;

}   // namespace pp

#endif  // CC_PREPROCESSOR_INPUT_H_
