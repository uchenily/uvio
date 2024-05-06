#include "uvio/log.hpp"
#include "uvio/task.hpp"

using namespace uvio;
using namespace uvio::log;

auto test() -> Task<> {
    console.info("run task ...");
    co_return;
}

auto main() -> int {
    auto task = test();
    auto handle = task.take();
    console.info("before resume");
    handle.resume();
    console.info("after resume");
}
