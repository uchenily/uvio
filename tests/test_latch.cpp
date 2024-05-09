#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/sync.hpp"

using namespace uvio;
using namespace uvio::sync;

auto latch_test(Latch &latch, std::atomic<std::size_t> &sum) -> Task<> {
    sum.fetch_add(1, std::memory_order::relaxed);
    co_await latch.arrive_and_wait();
}

auto test(std::size_t n) -> Task<> {
    assert(n >= 1);
    Latch latch{static_cast<std::ptrdiff_t>(n)};

    std::atomic<std::size_t> sum = 0;
    assert(latch.try_wait() == false);

    for (auto i = 0u; i < n - 1; i++) {
        spawn(latch_test(latch, sum));
    }
    sum.fetch_add(1, std::memory_order::relaxed);
    co_await latch.arrive_and_wait();
    console.info("expected: {}, actual {}", n, sum.load());
    assert(n == sum);
}

auto main() -> int {
    SET_LOG_LEVEL(LogLevel::INFO);
    block_on(test(100000));
}
