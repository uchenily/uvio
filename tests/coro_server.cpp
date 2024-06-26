#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

auto process(TcpStream stream) -> Task<> {
    console.info("process tcp stream ...");
    while (true) {
        std::array<char, 1024> buf{};

        auto rret = co_await stream.read(buf);
        if (!rret) {
            console.error(rret.error().message());
            break;
        }
        console.info("read from tcp stream: {}", buf.data());
        auto wret = co_await stream.write(buf);
        if (!wret) {
            console.error(wret.error().message());
            break;
        }
    }
    console.info("process tcp stream end.");
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
