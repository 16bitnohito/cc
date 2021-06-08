#include <preprocessor/logger.h>

#include <iostream>

using namespace std;

namespace pp {

// static
Logger& Logger::instance() {
	static Logger main_instance;
	return main_instance;
}

Logger::~Logger() {
}

void Logger::output_log_with_args(LogLevel level, const StringView& format,
	                              const std::format_args_t<LogOutputIterator, char>& args) {
	if (level < min_level_) {
		return ;
	}

	vformat_to(LogOutputIterator(cerr), format, args);
}

Logger::Logger()
	: min_level_(LogLevel::kWarning) {
}

}   // namespace pp
