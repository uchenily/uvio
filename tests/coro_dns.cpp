#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

auto test() -> Task<> {
    auto res = co_await DNS::resolve("baidu.com", "https");
    if (!res) {
        LOG_ERROR("{}", res.error().message());
        co_return;
    }
    LOG_INFO("{}", res.value());
    co_return;
}

auto main() -> int {
    block_on(test());
}
