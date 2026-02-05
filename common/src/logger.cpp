#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

namespace email {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(LogLevel level, bool console, const std::filesystem::path& file,
                  size_t max_file_size, size_t max_files) {
    std::lock_guard<std::mutex> lock(mutex_);

    level_ = level;
    console_ = console;
    max_file_size_ = max_file_size;
    max_files_ = max_files;

    if (!file.empty()) {
        log_file_ = file;
        // Create parent directories if they don't exist
        if (auto parent = file.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        file_stream_.open(file, std::ios::app);
        if (file_stream_.is_open()) {
            current_size_ = std::filesystem::file_size(file);
        }
    }
}

void Logger::trace(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Trace >= level_) {
        write(LogLevel::Trace, loc, std::string(msg));
    }
}

void Logger::debug(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Debug >= level_) {
        write(LogLevel::Debug, loc, std::string(msg));
    }
}

void Logger::info(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Info >= level_) {
        write(LogLevel::Info, loc, std::string(msg));
    }
}

void Logger::warning(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Warning >= level_) {
        write(LogLevel::Warning, loc, std::string(msg));
    }
}

void Logger::error(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Error >= level_) {
        write(LogLevel::Error, loc, std::string(msg));
    }
}

void Logger::fatal(std::string_view msg, const std::source_location& loc) {
    if (LogLevel::Fatal >= level_) {
        write(LogLevel::Fatal, loc, std::string(msg));
    }
}

void Logger::write(LogLevel level, const std::source_location& loc, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);

    // Extract just the filename from the full path
    std::filesystem::path file_path(loc.file_name());
    std::string filename = file_path.filename().string();

    std::string formatted = std::format("[{}] [{}] [{}:{}] {}",
                                        timestamp, level_str,
                                        filename, loc.line(),
                                        message);

    if (console_) {
        // Color output for console
        const char* color = "";
        const char* reset = "\033[0m";

        switch (level) {
            case LogLevel::Trace:   color = "\033[90m"; break;  // Gray
            case LogLevel::Debug:   color = "\033[36m"; break;  // Cyan
            case LogLevel::Info:    color = "\033[32m"; break;  // Green
            case LogLevel::Warning: color = "\033[33m"; break;  // Yellow
            case LogLevel::Error:   color = "\033[31m"; break;  // Red
            case LogLevel::Fatal:   color = "\033[35m"; break;  // Magenta
        }

        std::cerr << color << formatted << reset << "\n";
    }

    if (file_stream_.is_open()) {
        rotate_if_needed();
        file_stream_ << formatted << "\n";
        file_stream_.flush();
        current_size_ += formatted.length() + 1;
    }
}

void Logger::rotate_if_needed() {
    if (current_size_ < max_file_size_) return;

    file_stream_.close();

    // Rotate existing files
    for (size_t i = max_files_ - 1; i > 0; --i) {
        std::filesystem::path old_file = log_file_;
        old_file += "." + std::to_string(i);

        std::filesystem::path new_file = log_file_;
        new_file += "." + std::to_string(i + 1);

        if (std::filesystem::exists(old_file)) {
            if (i + 1 >= max_files_) {
                std::filesystem::remove(old_file);
            } else {
                std::filesystem::rename(old_file, new_file);
            }
        }
    }

    // Rename current log file
    std::filesystem::path rotated = log_file_;
    rotated += ".1";
    std::filesystem::rename(log_file_, rotated);

    // Open new file
    file_stream_.open(log_file_, std::ios::app);
    current_size_ = 0;
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
    }
    return "UNKNOWN";
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_now;
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

}  // namespace email
