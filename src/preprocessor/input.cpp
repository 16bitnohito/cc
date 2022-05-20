#include <preprocessor/input.h>

#include <cassert>
#include <sstream>
#include <vector>

using namespace std;

namespace pp {

#if HOST_PLATFORM == PLATFORM_WINDOWS
const String kPathDelimiter = T_("\\");
#else
const String kPathDelimiter = T_("/");
#endif


SourceFile::SourceFile(std::istream& input, const String& filepath, const Options& opts)
    : scanner_(input, opts.support_trigraphs())
    , path_(filepath)
    , condition_level_()
    , groups_()
    , line_(1) {
}

SourceFile::~SourceFile() {
}

Token SourceFile::next_token() {
    Token t = scanner_.next_token();
    if (line_ != t.line()) {
        line_ = t.line();
    }
    return t;
}

void SourceFile::scanner_hint(ScannerHint hint) {
    return scanner_.state_hint(hint);
}

String SourceFile::parent_dir() {
    String::size_type i = path_.rfind(kPathDelimiter);
    if (i == string::npos) {
        return T_("");
    } else {
        return path_.substr(0, (i - 0));
    }
}

void SourceFile::push_group(Group& group) {
    groups_.push(&group);
}

void SourceFile::pop_group() {
    groups_.pop();
}

std::uint32_t SourceFile::line() {
    return line_;
}

std::uint32_t SourceFile::column() {
    return scanner_.column();
}

void SourceFile::reset_line_number(std::uint32_t new_line_number) {
    // 分離した影響で、無理やり TokenStreamと分担させていて歪なのをどうにかしたい。
    scanner_.line_number(new_line_number);
    line_ = new_line_number;
}


SourceString::SourceString(const std::string& string, const Options& opts)
    : in_(string, ios_base::binary)
    , scanner_(in_, opts.support_trigraphs()) {
}

SourceString::~SourceString() {
}

Token SourceString::next_token() {
    return scanner_.next_token();
}


SourceTokenList::SourceTokenList(const TokenList& tokens)
    : tokens_ref_(tokens)
    , i_() {
}

SourceTokenList::~SourceTokenList() {
}

Token SourceTokenList::next_token() {
    if (i_ >= tokens_ref_.get().size()) {
        return kTokenEndOfFile;
    } else {
        Token result = tokens_ref_.get()[i_];
        ++i_;
        return result;
    }
}


TokenStream::TokenStream(Source& input_source)
    : input_source_(input_source)
    , lookahead_(kNumLookahead)
    , p_()
    , queue_()
    , q_() {
    for (size_t i = 0; i < lookahead_.size(); ++i) {
        consume();
    }
}

TokenStream::~TokenStream() {
    assert(q_ == queue_.size());
}

Source* TokenStream::input_source() const {
    return &input_source_.get();
}

void TokenStream::consume() {
    if (q_ < queue_.size()) {
        ++q_;
    } else {
        lookahead_[p_] = input_source_.get().next_token();
        p_ = (p_ + 1) % kNumLookahead;
    }
}

void TokenStream::insert(TokenList&& tokens) {
    queue_ = move(tokens);
    q_ = 0;
}

const Token& TokenStream::peek(int i) const {
    assert(i > 0);

    if (q_ < queue_.size()) {
        return queue_.at(q_ + (i - 1));
    } else {
        return lookahead_[(p_ + (i - 1)) % kNumLookahead];
    }
}

void TokenStream::reset_line_number(std::uint32_t new_line_number) {
    for (int i = 0; i < kNumLookahead; i++) {
        Token& t = lookahead_[(p_ + i) % kNumLookahead];
        t.line(new_line_number);

        if (t.type() == TokenType::kNewLine) {
            new_line_number++;
        }
    }
}

}   // namespace pp
