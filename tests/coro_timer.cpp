#include "uvio/core.hpp"
#include "uvio/time.hpp"

using namespace uvio;
using namespace uvio::time;

auto test() -> Task<> {
    co_await sleep(1s);
    console.info("sleep 1s");
    co_await sleep(1s);
    console.info("sleep 2s");
    co_await sleep(1s);
    console.info("sleep 3s");
    co_return;
}

auto main() -> int {
    block_on(test());
}
