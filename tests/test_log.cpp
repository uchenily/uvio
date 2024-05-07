#include "uvio/log.hpp"

using namespace uvio::log;

auto main() -> int {
    console.info("{:*^20}", "[log fg]");
    console.set_level(LogLevel::TRACE);
    console.trace("hello world");
    console.debug("hello world");
    console.info("hello world");
    console.warn("hello world");
    console.error("hello world");
    console.fatal("hello world");

    console.set_style(Style::Background);
    console.info("{:*^20}", "[log bg]");
    console.trace("hello world");
    console.debug("hello world");
    console.info("hello world");
    console.warn("hello world");
    console.error("hello world");
    console.fatal("hello world");
}
