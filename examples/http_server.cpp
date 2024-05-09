#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

constexpr std::string_view response = R"(HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 14

Hello, World!
)";

auto process(TcpStream stream) -> Task<> {
    std::array<char, 128> buf{};

    auto nread = co_await stream.read(buf);
    if (nread == UV_EOF) {
        co_return;
    }

    console.debug("<<< `{}`", buf.data());
    console.debug(">>> `{}`", response);
    co_await stream.write(response);
    co_return;
}

auto server() -> Task<> {
    std::string host{"localhost"};
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
