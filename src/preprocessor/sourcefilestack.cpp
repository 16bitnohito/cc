#include "preprocessor/sourcefilestack.h"

#include <stdexcept>

#include "preprocessor/input.h"

using namespace std;

namespace pp {

SourceFileStack::SourceFileStack() {
}

SourceFileStack::~SourceFileStack() {
}

void SourceFileStack::push(SourceFile* source) {
	if (!source) {
		throw invalid_argument(__func__);
	}
    sources_.push_back(source);
}

void SourceFileStack::pop() {
    sources_.pop_back();
}

SourceFile& SourceFileStack::current_source() const {
	if (sources_.empty()) {
		throw logic_error(__func__);
	}

    return *sources_.back();
}

SourceFile* SourceFileStack::current_source_pointer() const {
    return sources_.empty() ? nullptr : sources_.back();
}

String SourceFileStack::current_source_path() const {
	if (sources_.empty()) {
		return T_("<init>");
	} else {
		return current_source().source_path();
	}
}

void SourceFileStack::current_source_path(const String& value) {
	current_source().source_path(value);
}

std::uint32_t SourceFileStack::current_source_line_number() {
	if (sources_.empty()) {
		return 0;
	} else {
		return current_source().line();
	}
}

void SourceFileStack::enum_files(std::function<bool (SourceFile*)> callback) {
    for (auto it = sources_.rbegin(); it != sources_.rend(); ++it) {
        if (!callback(*it)) {
            break;
        }
    }
}

}
