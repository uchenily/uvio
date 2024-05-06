#include <format>
#include <iostream>
#include <string_view>

class ConsoleLogger {
public:
    template <typename... Args>
    void trace(std::string_view fmt, Args... args) {
        std::cout << "[TRACE] " << log(fmt, args...) << '\n';
    }

    template <typename... Args>
    void debug(std::string_view fmt, Args... args) {
        std::cout << "[DEBUG] " << log(fmt, args...) << '\n';
    }

    template <typename... Args>
    void info(std::string_view fmt, Args... args) {
        std::cout << "[INFO ] " << log(fmt, args...) << '\n';
    }

    template <typename... Args>
    void warn(std::string_view fmt, Args... args) {
        std::cout << "[WARN ] " << log(fmt, args...) << '\n';
    }

    template <typename... Args>
    void error(std::string_view fmt, Args... args) {
        std::cout << "[ERROR] " << log(fmt, args...) << '\n';
    }

private:
    template <typename... Args>
    auto log(std::string_view fmt, Args... args) {
        return std::vformat(fmt, std::make_format_args(args...));
    }
};

static inline auto console = ConsoleLogger();
