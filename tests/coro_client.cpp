#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

auto client() -> Task<> {
    std::string host{"localhost"};
    int         port{8000};
    std::string buf(1024, 0);

    auto stream = co_await TcpStream::connect(host, port);
    console.info("Connected to {}:{}", host, port);

    for (int i = 0; i < 3; i++) {
        co_await stream.write("hello");
        co_await stream.read(buf);
        console.debug("Received: `{}` {}", buf, i);
    }
}

auto main() -> int {
    block_on(client());
}
