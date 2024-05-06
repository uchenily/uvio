#include "uvio/log.hpp"

using namespace uvio::log;

auto main() -> int {
    console.set_level(LogLevel::TRACE);
    console.trace("hello world");
    console.debug("hello world");
    console.info("hello world");
    console.warn("hello world");
    console.error("hello world");
    console.fatal("hello world");
}
