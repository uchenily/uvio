#include "uvio/core.hpp"
#include "uvio/time.hpp"

using namespace uvio;
using namespace uvio::time;

auto task1() -> Task<> {
    for (int i = 0; i < 6; i++) {
        co_await sleep(1s);
        console.warn("task1 i={}", i);
    }
}

auto task2() -> Task<> {
    for (int i = 0; i < 3; i++) {
        co_await sleep(2s);
        console.info("task2 i={}", i);
    }
}

auto test() -> Task<> {
    spawn(task1());
    spawn(task2());
    co_return;
}

auto main() -> int {
    block_on(test());
}
