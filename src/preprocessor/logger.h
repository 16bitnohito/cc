#ifndef CC_PREPROCESSOR_LOGGER_H_
#define CC_PREPROCESSOR_LOGGER_H_

#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <preprocessor/config.h>
#include <preprocessor/utility.h>

namespace pp {

/**
 */
enum class LogLevel {
    kNoLog,
    kTrace,
    kDebug,
    kInfo,
    kWarning,
    kError,
    kFatal,
};

/**
 */
class Logger {
public:
    using LogOutputIterator = std::ostreambuf_iterator<char>;

    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;

    ~Logger();

    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    void set_min_level(LogLevel min_level) noexcept {
        min_level_ = min_level;
    }

    void set_output_stream(const std::shared_ptr<std::ostream>& output);

    void output_log_with_args(LogLevel level, const StringView& format,
                              const std::format_args& args);

    template <class... Ts>
    void output_log(LogLevel level, const StringView& format, const Ts&... args) {
        //using Context = std::basic_format_context<LogOutputIterator, char>;
        output_log_with_args(level, format, std::make_format_args(args...));
    }

    template <class... Ts>
    void fatal(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kFatal, format, args...);
    }

    template <class... Ts>
    void error(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kError, format, args...);
    }

    template <class... Ts>
    void warning(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kWarning, format, args...);
    }

    template <class... Ts>
    void info(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kInfo, format, args...);
    }

    template <class... Ts>
    void debug(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kDebug, format, args...);
    }

    template <class... Ts>
    void trace(const StringView& format, const Ts&... args) {
        output_log(LogLevel::kTrace, format, args...);
    }

protected:
    Logger();

private:
    LogLevel min_level_;
    std::shared_ptr<std::ostream> output_;
};

template <class... Ts>
inline
void log_fatal(const StringView& format, const Ts&... args) {
    pp::Logger::instance().fatal(format, args...);
}

template <class... Ts>
inline
void log_error(const StringView& format, const Ts&... args) {
    pp::Logger::instance().error(format, args...);
}

template <class... Ts>
inline
void log_warning(const StringView& format, const Ts&... args) {
    pp::Logger::instance().warning(format, args...);
}

template <class... Ts>
inline
void log_info(const StringView& format, const Ts&... args) {
    pp::Logger::instance().info(format, args...);
}

template <class... Ts>
inline
void log_debug(const StringView& format, const Ts&... args) {
    pp::Logger::instance().debug(format, args...);
}

template <class... Ts>
inline
void log_trace(const StringView& format, const Ts&... args) {
    pp::Logger::instance().trace(format, args...);
}

}	// namespace pp

#endif  // CC_PREPROCESSOR_LOGGER_H_
