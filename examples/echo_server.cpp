#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

// Ignore errors
auto process(TcpStream stream) -> Task<> {
    while (true) {
        std::array<char, 1024> buf{};

        if (auto ret = co_await stream.read(buf); !ret) {
            console.error(ret.error().message());
            break;
        }
        console.info("Received: `{}`", buf.data());
        if (auto ret = co_await stream.write(buf); !ret) {
            console.error(ret.error().message());
            break;
        }
    }
    co_return;
}

auto server() -> Task<> {
    std::string host{"0.0.0.0"};
    int         port{8000};

    auto listener = TcpListener();
    listener.bind(host, port);
    console.info("Listening on {}:{} ...", host, port);
    while (true) {
        auto stream = (co_await listener.accept()).value();
        spawn(process(std::move(stream)));
    }
}

auto main() -> int {
    block_on(server());
}
