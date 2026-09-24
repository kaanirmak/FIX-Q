#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace logger {

enum class Level {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

inline std::string level_to_string(Level level) {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO ";
        case Level::WARN:  return "WARN ";
        case Level::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// Thread-safe centralized logger
class Logger {
private:
    std::mutex mtx_;
    Level min_level_;

    Logger() : min_level_(Level::INFO) {}

public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(Level level) {
        std::lock_guard<std::mutex> lock(mtx_);
        min_level_ = level;
    }

    void log(Level level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (level < min_level_) return;

        // Get current time with microsecond resolution
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto duration = now.time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

        std::tm buf;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif

        std::ostream& out = (level == Level::ERROR) ? std::cerr : std::cout;

        out << "[" << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") 
            << "." << std::setw(6) << std::setfill('0') << micros << "] "
            << "[" << level_to_string(level) << "] "
            << "[TID:" << std::this_thread::get_id() << "] "
            << message << std::endl;
    }
};

inline void log_debug(const std::string& msg) { Logger::instance().log(Level::DEBUG, msg); }
inline void log_info(const std::string& msg)  { Logger::instance().log(Level::INFO, msg); }
inline void log_warn(const std::string& msg)  { Logger::instance().log(Level::WARN, msg); }
inline void log_error(const std::string& msg) { Logger::instance().log(Level::ERROR, msg); }

} // namespace logger

#endif // LOGGER_HPP
