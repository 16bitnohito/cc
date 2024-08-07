#include "input.h"

#include <cassert>
#include <sstream>
#include <vector>

#include "sourcefilestack.h"

using namespace std;

namespace pp {

#if HOST_PLATFORM == PLATFORM_WINDOWS
const String kPathDelimiter = T_("\\");
#else
const String kPathDelimiter = T_("/");
#endif


const Char* IncludeDir::type_string(Type type) {
    switch (type) {
    case IncludeDir::kSystem:
        return T_("kSystem");
    case IncludeDir::kUser:
        return T_("kUser");
    case IncludeDir::kSource:
        return T_("kSource");
    default:
        assert(false);
        return T_("(null)");
    }
}


SourceFile::SourceFile(std::istream& input, const String& filepath, const IncludeDir& include_dir, const Options& opts, Diagnostics& diag, SourceFileStack& sources)
    : sources_(sources)
    , scanner_(input, opts.support_trigraphs(), diag, sources)
    , path_(filepath)
    , include_dir_(include_dir)
    , condition_level_()
    , groups_()
    , line_(1)
    , is_system_file_() {
    sources_.push(this);
    sources_.enum_files(
           [this](SourceFile* s) {
                if (s->include_dir().type() == IncludeDir::kSystem) {
                    is_system_file_ = true;
                    return false;
                }
                return true;
            });
}
 
SourceFile::~SourceFile() {
    sources_.pop();
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
    if (path_ == T_("-")) {
        return include_dir().path();
    } else {
        String::size_type i = path_.rfind(kPathDelimiter);
        if (i == string::npos) {
            throw logic_error(__func__);
        }
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


SourceString::SourceString(const std::string& string, const Options& opts, Diagnostics& diag, SourceFileStack& sources)
    : in_(string, ios_base::binary)
    , scanner_(in_, opts.support_trigraphs(), diag, sources) {
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
