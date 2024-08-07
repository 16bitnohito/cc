#ifndef CC_PREPROCESSOR_INPUT_H_
#define CC_PREPROCESSOR_INPUT_H_

#include <sstream>
#include <stack>

#include "util/utility.h"

#include "pp_config.h"
#include "diagnostics.h"
#include "options.h"
#include "scanner.h"
#include "token.h"

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
class IncludeDir {
public:
    /**
     * 便宜的なインクルードパスの分類です。
     */
    enum Type {
        // 環境変数の INCLUDEで指定されているものを、システムのインクルードディレクトリ扱いにします。
        kSystem,
        // コマンドラインオプションの Iで指定されるものは、これになります。
        kUser,
        // 最初のソースファイルと、ダブルクオート形式で指定されたヘッダーファイルが見つかったディレクトリは、これで扱います。
        kSource,
    };

    static const Char* type_string(Type header_type);

    IncludeDir()
        : type_()
        , path_() {
    }

    explicit IncludeDir(Type type, const String& path)
        : type_(type)
        , path_(path) {
    }

    const String& path() const {
        return path_;
    }

    Type type() const {
        return type_;
    }

private:
    Type type_;
    String path_;
};

class SourceFileStack;

/**
 */
class SourceFile
    : public Source {
public:
    friend class ConditionScope;

    explicit SourceFile(std::istream& input, const String& filepath, const IncludeDir& include_dir, const Options& opts, Diagnostics& diag, SourceFileStack& sources);
    SourceFile(const SourceFile&) = delete;
    virtual ~SourceFile() override;

    SourceFile& operator=(const SourceFile&) = delete;

    virtual Token next_token() override;

    void scanner_hint(ScannerHint hint);

    bool is_system_file() const { return is_system_file_; }

    const String& source_path() const { return path_; }
    void source_path(const String& value) { path_ = value; }

    int condition_level() const { return condition_level_; }
    void inc_condition_level() { ++condition_level_; }
    void dec_condition_level() { --condition_level_; }

    String parent_dir();
    const IncludeDir& include_dir() const { return include_dir_; }
    std::size_t num_groups() const { return groups_.size(); }
    Group* current_group() const { return groups_.top(); }

    void reset_line_number(std::uint32_t new_line_number);

    void push_group(Group& group);
    void pop_group();

    std::uint32_t line();
    std::uint32_t column();

private:
    SourceFileStack& sources_;
    Scanner scanner_;
    String path_;
    IncludeDir include_dir_;
    int condition_level_;
    std::stack<Group*> groups_;
    std::uint32_t line_;
    bool is_system_file_;
};

/**
 */
class SourceString
    : public Source {
public:
    explicit SourceString(const std::string& string, const Options& opts, Diagnostics& diag, SourceFileStack& sources);
    SourceString(const SourceString&) = delete;
    virtual ~SourceString() override;

    SourceString& operator=(const SourceString&) = delete;

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
    SourceTokenList(const SourceTokenList&) = delete;
    virtual ~SourceTokenList() override;

    SourceTokenList& operator=(const SourceTokenList&) = delete;

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
