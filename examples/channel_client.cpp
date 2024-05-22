#include "uvio/codec.hpp"
#include "uvio/core.hpp"
#include "uvio/log.hpp"
#include "uvio/net.hpp"
#include "uvio/time.hpp"

#include "tcp_channel.hpp"

using namespace uvio;
using namespace uvio::log;
using namespace uvio::net;
using namespace uvio::codec;

using namespace example;

auto process(TcpStream &&stream) -> Task<> {
    Channel channel{std::move(stream)};

    for (auto i = 0u; i < 64; i++) {
        co_await channel.Send(std::format("client message round {}", i));
        auto message = (co_await channel.Recv()).value();
        console.info("Received: `{}`", message);
    }
    // co_await channel.Close();
}

auto client() -> Task<> {
    auto ret = co_await TcpStream::connect("127.0.0.1", 9999);
    if (!ret) {
        console.error("{}", ret.error().message());
        co_return;
    }
    auto stream = std::move(ret.value());
    co_await process(std::move(stream));
}

auto main() -> int {
    block_on(client());
}
