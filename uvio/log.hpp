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
        std::clog << std::format("[{:<5}] {}:{} {}\n",
                                 to_string(Level),
                                 source_location.file_name(),
                                 source_location.line(),
                                 message);
    }
};

static inline auto console = ConsoleLogger();
