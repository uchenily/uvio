#include "uvio/log.hpp"
#include "uvio/runtime.hpp"
#include "uvio/task.hpp"
#include "uvio/time/sleep.hpp"

using namespace uvio;
using namespace uvio::log;
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
