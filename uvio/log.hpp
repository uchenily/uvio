#include <format>
#include <iostream>
#include <string_view>

class ConsoleLogger {
public:
    template <typename... Args>
    void trace(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << "[TRACE] " << msg << '\n';
    }

    template <typename... Args>
    void debug(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << "[DEBUG] " << msg << '\n';
    }

    template <typename... Args>
    void info(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << "[INFO ] " << msg << '\n';
    }

    template <typename... Args>
    void warn(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << "[WARN ] " << msg << '\n';
    }

    template <typename... Args>
    void error(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << "[ERROR] " << msg << '\n';
    }
};

static inline auto console = ConsoleLogger();
