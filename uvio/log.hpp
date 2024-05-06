#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

namespace uvio::log {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

namespace detail {
    constexpr auto to_string(LogLevel level) noexcept -> std::string_view {
        using enum LogLevel;
        switch (level) {
        case TRACE:
            return "TRACE";
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO ";
        case WARN:
            return "WARN ";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    constexpr auto to_color(LogLevel level) noexcept -> std::string_view {
        using enum LogLevel;
        switch (level) {
        case TRACE:
            return "\033[36m";
        case DEBUG:
            return "\033[94m";
        case INFO:
            return "\033[32m";
        case WARN:
            return "\033[33m";
        case ERROR:
            return "\033[31m";
        case FATAL:
            return "\033[35m";
        default:
            return "";
        }
    }

    constexpr auto source_color() noexcept -> std::string_view {
        return "\033[90m";
    }

    constexpr auto reset_color() noexcept -> std::string_view {
        return "\033[0m";
    }

    template <int N, char c>
    inline auto to_int(uint64_t num, char *p, int &size) {
        constexpr static std::array<char, 10> digits
            = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

        for (int i = 0; i < N; i++) {
            p[--size] = digits[num % 10];
            num = num / 10;
        }

        if constexpr (N != 4) {
            p[--size] = c;
        }
    }

    inline auto get_timestamp(const std::chrono::system_clock::time_point &now)
        -> char * {
        static thread_local std::array<char, 33> buf{};
        static thread_local std::chrono::seconds last_second{};

        std::chrono::system_clock::duration duration = now.time_since_epoch();
        std::chrono::seconds                seconds
            = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto milliseconds
            = std::chrono::duration_cast<std::chrono::milliseconds>(duration
                                                                    - seconds)
                  .count();
        int size = 23;
        if (last_second == seconds) {
            to_int<3, '.'>(milliseconds, buf.data(), size);
            return buf.data();
        }

        last_second = seconds;
        auto tt = std::chrono::system_clock::to_time_t(now);
        auto tm = localtime(&tt);

        to_int<3, '.'>(milliseconds, buf.data(), size);
        to_int<2, ':'>(tm->tm_sec, buf.data(), size);
        to_int<2, ':'>(tm->tm_min, buf.data(), size);
        to_int<2, ' '>(tm->tm_hour, buf.data(), size);

        to_int<2, '-'>(tm->tm_mday, buf.data(), size);
        to_int<2, '-'>(tm->tm_mon + 1, buf.data(), size);
        to_int<4, ' '>(tm->tm_year + 1900, buf.data(), size);
        return buf.data();
    }

    class FmtWithSourceLocation {
    public:
        template <typename T>
            requires std::constructible_from<std::string_view, T>
        FmtWithSourceLocation(T                  &&fmt,
                              std::source_location location
                              = std::source_location::current())
            : fmt_{std::forward<T>(fmt)}
            , source_location_{location} {}

    public:
        auto fmt() {
            return fmt_;
        }

        auto source_location() {
            return source_location_;
        }

    private:
        std::string_view     fmt_;
        std::source_location source_location_;
    };

    class ConsoleLogger {
    public:
        template <typename... Args>
        auto trace(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::TRACE>(fwsl, std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto debug(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::DEBUG>(fwsl, std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto info(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::INFO>(fwsl, std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto warn(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::WARN>(fwsl, std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto error(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::ERROR>(fwsl, std::forward<Args>(args)...);
        }

        template <typename... Args>
        auto fatal(FmtWithSourceLocation fwsl, Args &&...args) {
            log<LogLevel::FATAL>(fwsl, std::forward<Args>(args)...);
        }

        auto set_level(LogLevel level) {
            level_ = level;
        }

    private:
        template <LogLevel Level, typename... Args>
        auto log(FmtWithSourceLocation fwsl, Args &...args) {
            if (Level < level_) {
                return;
            }
            auto fmt = fwsl.fmt();
            auto source_location = fwsl.source_location();
            auto message = std::vformat(fmt, std::make_format_args(args...));
            auto now = std::chrono::system_clock::now();
            std::clog << std::format("{} |{}{:<5}{}| {}{}:{}{} {}\n",
                                     get_timestamp(now),
                                     to_color(Level),
                                     to_string(Level),
                                     reset_color(),
                                     source_color(),
                                     source_location.file_name(),
                                     source_location.line(),
                                     reset_color(),
                                     message);
        }

    private:
        LogLevel level_{LogLevel::DEBUG};
    };

} // namespace detail

static inline auto console = detail::ConsoleLogger();
} // namespace uvio::log
