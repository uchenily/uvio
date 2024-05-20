#include "uvio/core.hpp"
#include "uvio/io.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;
using namespace uvio::io;

// auto process(TcpStream stream) -> Task<> {
//     // BufStream<TcpStream, 1, 1> buffered_stream(std::move(stream));
//     BufStream<TcpStream, 1024, 1024> buffered_stream(std::move(stream));
//     while (true) {
//         std::array<char, 6> small_buf{};
//
//         auto rret = co_await buffered_stream.read_exact(small_buf);
//         if (!rret) {
//             console.error(rret.error().message());
//             break;
//         }
//
//         console.info("Received: `{}`", small_buf.data());
//         auto wret = co_await buffered_stream.write(small_buf);
//         if (!wret) {
//             console.error(wret.error().message());
//             break;
//         }
//
//         co_await buffered_stream.flush();
//     }
// }

auto process(TcpStream stream) -> Task<> {
    // BufStream<TcpStream, 1, 1> buffered_stream(std::move(stream));
    BufStream<TcpStream, 1024, 1024> buffered_stream(std::move(stream));
    while (true) {
        std::string buf;

        auto rret = co_await buffered_stream.read_until(buf, "XXX");
        if (!rret) {
            console.error(rret.error().message());
            break;
        }

        console.info("Received: `{}`", buf.data());
        auto wret = co_await buffered_stream.write(buf);
        if (!wret) {
            console.error(wret.error().message());
            break;
        }

        co_await buffered_stream.flush();
    }
}

auto test() -> Task<> {
    auto listener = TcpListener();
    listener.bind("127.0.0.1", 8000);
    while (true) {
        auto stream = (co_await listener.accept()).value();
        spawn(process(std::move(stream)));
    }
}

auto main() -> int {
    block_on(test());
}
