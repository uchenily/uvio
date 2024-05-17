#include "uvio/core.hpp"
#include "uvio/io.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;
using namespace uvio::io;

auto process(TcpStream stream) -> Task<> {
    // BufStream<TcpStream, 1, 1> buffered_stream(std::move(stream));
    BufStream<TcpStream, 1024, 1024> buffered_stream(std::move(stream));
    int                              count = 0;
    while (true) {
        std::array<char, 64> small_buf{};

        auto nread = co_await buffered_stream.read(small_buf);
        if (nread < 0) {
            break;
        }
        count++;

        console.info("Received: `{}`", small_buf.data());
        auto nwritten = co_await buffered_stream.write(
            {small_buf.data(), static_cast<std::size_t>(nread)});
        if (nwritten == 0) {
            break;
        }

        // read twice before flushing
        if (count % 2 == 0) {
            co_await buffered_stream.flush();
        }
    }
}

auto test() -> Task<> {
    auto listener = TcpListener();
    listener.bind("localhost", 8000);
    while (true) {
        auto stream = co_await listener.accept();
        spawn(process(std::move(stream)));
    }
}

auto main() -> int {
    block_on(test());
}
