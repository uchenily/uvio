#include <format>
#include <iostream>
#include <string_view>

class ConsoleLogger {
public:
    template <typename... Args>
    void info(std::string_view fmt, Args... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        std::cout << msg << '\n';
    }
};

static inline auto console = ConsoleLogger();
