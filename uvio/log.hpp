#include <chrono>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
};

constexpr auto to_string(LogLevel level) -> std::string_view {
    switch (level) {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO ";
    case LogLevel::WARN:
        return "WARN ";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

template <int N, char c>
inline void to_int(uint64_t num, char *p, int &size) {
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
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            duration - seconds)
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
    void trace(FmtWithSourceLocation fwsl, Args... args) {
        log<LogLevel::TRACE>(fwsl, args...);
    }

    template <typename... Args>
    void debug(FmtWithSourceLocation fwsl, Args... args) {
        log<LogLevel::DEBUG>(fwsl, args...);
    }

    template <typename... Args>
    void info(FmtWithSourceLocation fwsl, Args... args) {
        log<LogLevel::INFO>(fwsl, args...);
    }

    template <typename... Args>
    void warn(FmtWithSourceLocation fwsl, Args... args) {
        log<LogLevel::WARN>(fwsl, args...);
    }

    template <typename... Args>
    void error(FmtWithSourceLocation fwsl, Args... args) {
        log<LogLevel::ERROR>(fwsl, args...);
    }

private:
    template <LogLevel Level, typename... Args>
    auto log(FmtWithSourceLocation fwsl, Args... args) {
        auto fmt = fwsl.fmt();
        auto source_location = fwsl.source_location();
        auto message = std::vformat(fmt, std::make_format_args(args...));
        auto now = std::chrono::system_clock::now();
        std::clog << std::format("{} [{:<5}] {}:{} {}\n",
                                 get_timestamp(now),
                                 to_string(Level),
                                 source_location.file_name(),
                                 source_location.line(),
                                 message);
    }
};

static inline auto console = ConsoleLogger();
