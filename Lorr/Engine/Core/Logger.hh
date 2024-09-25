#pragma once

#include "Engine/Memory/Stack.hh"

#include <source_location>

namespace lr {
namespace Logger {
    enum Category { DBG = 0, INF, WRN, ERR, CRT };
    constexpr static std::string_view LOG_CATEGORY_STR[] = { "DBG", "INF", "WRN", "ERR", "CRT" };
    constexpr static std::string_view LOG_CATEGORY_COLORS[] = { "\033[90m", "\033[32m", "\033[33m", "\033[31m", "\033[31m" };

    void init(std::string_view name);
    void to_file(std::string_view str);
}  // namespace Logger

struct LoggerFmt : public std::string_view {
    std::source_location location;

    template<typename String>
        requires std::constructible_from<std::string_view, String>
    consteval LoggerFmt(const String &string, const std::source_location location = std::source_location::current()) noexcept
        : std::string_view(string),
          location(location) {}

    template<std::formattable<char>... Args>
    constexpr const std::format_string<Args...> &get() const noexcept {
        return reinterpret_cast<const std::format_string<Args...> &>(*this);
    }
};

template<typename... ArgsT>
constexpr static void LOG(Logger::Category cat, const LoggerFmt &fmt, ArgsT &&...args) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto *tm = std::localtime(&time);
    auto msg = stack.format(fmt.get<ArgsT...>(), args...);
    auto full_msg = stack.format(
        "{}{:04}-{:02}-{:02} {:02}:{:02}:{:02} | {} | {}:{}: {}\033[0m\n",
        Logger::LOG_CATEGORY_COLORS[cat],
        // YYYY-MM-DD
        tm->tm_year + 1900,
        tm->tm_mon + 1,
        tm->tm_mday,
        // HH-MM-SS
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec,
        // CAT
        Logger::LOG_CATEGORY_STR[cat],
        // F:L
        fmt.location.file_name(),
        fmt.location.line(),
        // MSG
        msg);

    Logger::to_file(full_msg);
}

template<typename... ArgsT>
constexpr static void LOG_TRACE(const LoggerFmt &fmt, ArgsT &&...args) {
    LOG(Logger::Category::DBG, fmt, args...);
}

template<typename... ArgsT>
constexpr static void LOG_INFO(const LoggerFmt &fmt, ArgsT &&...args) {
    LOG(Logger::Category::INF, fmt, args...);
}

template<typename... ArgsT>
constexpr static void LOG_WARN(const LoggerFmt &fmt, ArgsT &&...args) {
    LOG(Logger::Category::WRN, fmt, args...);
}

template<typename... ArgsT>
constexpr static void LOG_ERROR(const LoggerFmt &fmt, ArgsT &&...args) {
    LOG(Logger::Category::ERR, fmt, args...);
}

template<typename... ArgsT>
constexpr static void LOG_FATAL(const LoggerFmt &fmt, ArgsT &&...args) {
    LOG(Logger::Category::CRT, fmt, args...);
    LS_DEBUGBREAK();
}

}  // namespace lr
