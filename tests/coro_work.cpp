#include "uvio/core.hpp"
#include "uvio/time.hpp"

#include <thread>

using namespace uvio;
using namespace uvio::time;

auto block_task() {
    console.info("block task start");
    std::this_thread::sleep_for(5s);
    console.info("block task end");
}

auto print_task() -> Task<> {
    for (int i = 0; i < 10; i++) {
        co_await sleep(1s);
        console.info("tick {}", i);
    }
}

auto test() -> Task<> {
    spawn(block_task);
    spawn(print_task());
    co_return;
}

auto main() -> int {
    block_on(test());
}
