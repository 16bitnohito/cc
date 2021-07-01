#include <preprocessor/logger.h>

#include <iostream>

using namespace std;

namespace pp {

Logger& Logger::instance() {
	static Logger main_instance;
	return main_instance;
}

Logger::~Logger() {
}

void Logger::set_output_stream(const std::shared_ptr<std::ostream>& output) {
	output_ = output;
}

void Logger::output_log_with_args(LogLevel level, const StringView& format,
	                              const std::format_args& args) {
	if (level < min_level_) {
		return ;
	}

	//vformat_to(BufferedOutputIterator(cerr), format, args);
	auto s = vformat(format, args);
	if (output_) {
		output_->write(s.data(), ssize(s));
	} else {
		cerr.write(s.data(), ssize(s));
	}
}

Logger::Logger()
	: min_level_(LogLevel::kWarning)
	, output_() {
}

}   // namespace pp
