#include "uvio/core.hpp"

using namespace uvio;

auto test() -> Task<> {
    console.info("run task ...");
    co_return;
}

auto test_call3() -> Task<> {
    console.info("test_call3");
    co_return;
}

auto test_call2() -> Task<> {
    console.info("test_call2 start");
    co_await test_call3();
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
