#include "uvio/core.hpp"
#include "uvio/log.hpp"
#include "uvio/net.hpp"

#include "tcp_channel.hpp"

using namespace uvio;
using namespace uvio::net;
using namespace uvio::log;
using namespace uvio::codec;

using namespace example;

auto process(TcpStream &&stream) -> Task<void> {
    Channel channel{std::move(stream)};

    for (auto i = 0u; i < 64; i++) {
        co_await channel.Send(std::format("server message round {}", i));
        auto message = (co_await channel.Recv()).value();
        console.info("Received: `{}`", message);
    }
    // co_await channel.Close();
}

auto server() -> Task<void> {
    std::string host = "127.0.0.1";
    int         port = 9999;

    auto listener = TcpListener();
    listener.bind(host, port);
    console.info("Listening on {}:{} ...", host, port);
    while (true) {
        auto has_stream = co_await listener.accept();

        if (has_stream) {
            auto stream = std::move(has_stream.value());
            spawn(process(std::move(stream)));
        } else {
            console.error(has_stream.error().message());
            break;
        }
    }
}

auto main() -> int {
    block_on(server());
}
