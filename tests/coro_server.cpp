#include "uvio/net/tcp_listener.hpp"
#include "uvio/runtime.hpp"
#include "uvio/task.hpp"

using namespace uvio;

auto process(TcpStream stream) -> Task<> {
    console.info("process tcp stream ...");
    while (true) {
        std::array<char, 1024> buf{};

        auto nread = co_await stream.read(buf);
        if (nread == UV_EOF) {
            console.info("EOF");
            break;
        }
        console.info("read from tcp stream: {}", buf.data());
        co_await stream.write(buf.data());
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
        auto stream = co_await listener.accept();
        spawn(process(std::move(stream)));
    }
}

auto main() -> int {
    block_on(server());
}
