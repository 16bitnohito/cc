#include <preprocessor/input.h>

#include <cassert>
#include <sstream>
#include <vector>

using namespace std;

namespace {

using namespace pp;

class TokenStreamFromTokenList
    : public TokenStream::Buffer {
public:
    explicit TokenStreamFromTokenList(const TokenList& tokens)
        : tokens_ref_(tokens)
        , i_() {
    }

    virtual ~TokenStreamFromTokenList() override {
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
    explicit TokenStreamFromString(const std::string& string, const Options& opts)
        : in_(string)
        , scanner_(&in_, opts.support_trigraphs(), false)
        , lookahead_(scanner_.next_token()) {
    }

    virtual ~TokenStreamFromString() override {
    }

    virtual void consume() override {
        lookahead_ = scanner_.next_token();
    }

    virtual const Token& peek(int /*i*/) const override {
        return lookahead_;
    }

private:
    std::istringstream in_;
    Scanner scanner_;
    //  XXX: 現状 1つしか先読みしないので、Sourceでやっているのと同じではない。別の方法にした方が良い。
    Token lookahead_;
};

class TokenStreamFromIStream
    : public TokenStream::Buffer {
public:
    explicit TokenStreamFromIStream(std::istream* input, const Options& opts)
        : scanner_(input, opts.support_trigraphs())
        , lookahead_(Source::kNumLookahead)
        , p_() {
        for (int i = 0; i < Source::kNumLookahead; ++i) {
            consume();
        }
    }

    TokenStreamFromIStream(const TokenStreamFromIStream&) = delete;

    virtual ~TokenStreamFromIStream() override {
    }

    TokenStreamFromIStream& operator=(const TokenStreamFromIStream&) = delete;

    virtual void consume() override {
        lookahead_[p_] = scanner_.next_token();
        p_ = (p_ + 1) % Source::kNumLookahead;

        // const Token& t = lookahead[p];
        // if (line != t.line()) {
        //     line = t.line();
        // }
    }

    virtual const Token& peek(int i) const override {
        return lookahead_[(p_ + i - 1) % Source::kNumLookahead];
    }

private:
    Scanner scanner_;
    std::vector<Token> lookahead_;
    int p_;
};

}   // anonymous namespace

namespace pp {

#if HOST_PLATFORM == PLATFORM_WINDOWS
const std::string kPathDelimiter = "\\";
#else
const std::string kPathDelimiter = "/";
#endif

Source::Source(const std::string& filepath, Scanner* src_scanner) {
	path_ = filepath;
	line_ = 1;
	scanner_ = src_scanner;
	lookahead_.resize(kNumLookahead);
	p_ = 0;
	condition_level_ = 0;
	//queue_;
	q_ = 0;

	for (int i = 0; i < kNumLookahead; ++i) {
		consume();
	}
}

Source::~Source() {
	assert(q_ == queue_.size());
}

std::string Source::parent_dir() {
	string::size_type i = path_.rfind(kPathDelimiter);
	if (i == string::npos) {
		return "";
	} else {
		return path_.substr(0, (i - 0));
	}
}

void Source::consume() {
	if (q_ < queue_.size()) {
		q_++;
	} else {
		lookahead_[p_] = scanner_->next_token();
		p_ = (p_ + 1) % kNumLookahead;

		const Token& t = lookahead_[p_];
		if (line_ != t.line()) {
			line_ = t.line();
		}
	}
}

void Source::insert(TokenList&& tokens) {
	queue_ = move(tokens);
	q_ = 0;
}

const Token& Source::peek(int i) {
	if (q_ < queue_.size()) {
		// XXX: q + (i - 1) >= queue.size()の場合
		return queue_[q_ + (i - 1)];
	} else {
		return lookahead_[(p_ + i - 1) % kNumLookahead];
	}
}

void Source::reset_line_number(uint32_t new_line_number) {
	for (int i = 0; i < kNumLookahead; i++) {
		Token& t = lookahead_[(p_ + i) % kNumLookahead];
		t.line(new_line_number);

		if (t.type() == TokenType::kNewLine) {
			new_line_number++;
		}
	}
	scanner_->line_number(new_line_number);
	line_ = new_line_number;
}

void Source::push_group(Group* group) {
	groups_.push(group);
}

void Source::pop_group() {
	groups_.pop();
}


TokenStream::TokenStream(const TokenList& tokens)
    : buffer_(std::make_unique<TokenStreamFromTokenList>(tokens))
    , queue_()
    , q_() {
}

TokenStream::TokenStream(const std::string& string, const Options& opts)
    : buffer_(std::make_unique<TokenStreamFromString>(string, opts))
    , queue_()
    , q_() {
}

TokenStream::TokenStream(std::istream* input, const Options& opts)
    : buffer_(std::make_unique<TokenStreamFromIStream>(input, opts))
    , queue_()
    , q_() {
}

// TokenStream::TokenStream(FILE* file, const Options& opts)
//     : buffer_(std::make_unique<TokenStreamFromFile>(file, opts))
//     , queue_()
//     , q_() {
// }

TokenStream::~TokenStream() {
    *buffer_ = *buffer_;
}

void TokenStream::consume() {
    if (q_ < queue_.size()) {
        ++q_;
    } else {
        buffer_->consume();
    }
}

void TokenStream::insert(TokenList&& tokens) {
    queue_ = move(tokens);
    q_ = 0;
}

const Token& TokenStream::peek(int i) const {
    if (q_ < queue_.size()) {
        // XXX: q_ + (i - 1) >= queue_.size()の場合
        return queue_[q_ + (i - 1)];
    } else {
        return buffer_->peek(i);
    }
}

}   // namespace pp
