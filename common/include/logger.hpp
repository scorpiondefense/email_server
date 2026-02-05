#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <fstream>
#include <chrono>
#include <format>
#include <source_location>
#include <filesystem>

namespace email {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

class Logger {
public:
    static Logger& instance();

    void init(LogLevel level = LogLevel::Info,
              bool console = true,
              const std::filesystem::path& file = "",
              size_t max_file_size = 10 * 1024 * 1024,
              size_t max_files = 5);

    void set_level(LogLevel level) { level_ = level; }
    LogLevel level() const { return level_; }

    template<typename... Args>
    void log(LogLevel level, const std::source_location& loc,
             std::format_string<Args...> fmt, Args&&... args) {
        if (level < level_) return;

        std::string message = std::format(fmt, std::forward<Args>(args)...);
        write(level, loc, message);
    }

    void trace(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void debug(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void info(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void warning(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void error(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void fatal(std::string_view msg, const std::source_location& loc = std::source_location::current());

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write(LogLevel level, const std::source_location& loc, const std::string& message);
    void rotate_if_needed();
    std::string level_to_string(LogLevel level) const;
    std::string get_timestamp() const;

    LogLevel level_ = LogLevel::Info;
    bool console_ = true;
    std::filesystem::path log_file_;
    std::ofstream file_stream_;
    size_t max_file_size_ = 10 * 1024 * 1024;
    size_t max_files_ = 5;
    size_t current_size_ = 0;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_TRACE(msg) email::Logger::instance().trace(msg)
#define LOG_DEBUG(msg) email::Logger::instance().debug(msg)
#define LOG_INFO(msg) email::Logger::instance().info(msg)
#define LOG_WARNING(msg) email::Logger::instance().warning(msg)
#define LOG_ERROR(msg) email::Logger::instance().error(msg)
#define LOG_FATAL(msg) email::Logger::instance().fatal(msg)

// Format macros
#define LOG_TRACE_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Trace, std::source_location::current(), fmt, __VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Debug, std::source_location::current(), fmt, __VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Info, std::source_location::current(), fmt, __VA_ARGS__)
#define LOG_WARNING_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Warning, std::source_location::current(), fmt, __VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Error, std::source_location::current(), fmt, __VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...) \
    email::Logger::instance().log(email::LogLevel::Fatal, std::source_location::current(), fmt, __VA_ARGS__)

}  // namespace email
