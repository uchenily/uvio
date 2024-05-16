#include "uvio/core.hpp"

using namespace uvio;

auto test() -> Task<> {
    console.info("run task ...");
    co_return;
}

auto test_call3() -> Task<std::string> {
    co_return std::string{"hello"};
}

auto test_call4() {
    console.info("test_call4");
    struct Awaiter {
        auto await_ready() const noexcept -> bool {
            return true;
        }
        auto await_suspend(
            [[maybe_unused]] std::coroutine_handle<> handle) noexcept {}
        [[nodiscard]]
        auto await_resume() noexcept {
            return std::string{"world"};
        }
    };
    return Awaiter{};
}

auto test_call2() -> Task<> {
    console.info("test_call2 start");
    auto res = co_await test_call3();
    console.info("co_await test_call3() res: {}", res);
    auto res2 = co_await test_call4();
    console.info("co_await test_call4() res: {}", res2);
    console.info("test_call2 end");
}

auto test_call() -> Task<> {
    console.info("test_call1 start");
    co_await test_call2();
    console.info("test_call1 end");
}

auto main() -> int {
    auto task = test();
    auto handle = task.take();
    console.info("before resume");
    handle.resume();
    console.info("after resume");

    auto task2 = test_call();
    auto handle2 = task2.take();
    handle2.resume();
}
