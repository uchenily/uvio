#include "uvio/core.hpp"
#include "uvio/net.hpp"
#include "uvio/sync.hpp"

#include <thread>

using namespace uvio;
using namespace uvio::sync;
using namespace uvio::net;

using namespace std::literals;

auto server(Latch &start, Latch &finish) -> Task<> {

    std::array<char, 64> buf{};

    auto listener = TcpListener();
    listener.bind("127.0.0.1", 12345);
    start.count_down();

    auto                  stream = (co_await listener.accept()).value();
    [[maybe_unused]] auto nread = (co_await stream.read(buf)).value();
    co_await stream.write(buf);

    finish.count_down();
};

auto client(Latch &start, Latch &finish) -> Task<> {
    std::this_thread::sleep_for(3s);
    std::array<char, 64> buf{};

    co_await start.arrive_and_wait();
    auto stream = (co_await TcpStream::connect("127.0.0.1", 12345)).value();
    co_await stream.write("test message");
    [[maybe_unused]] auto nread = (co_await stream.read(buf)).value();

    finish.count_down();
};

auto test() -> Task<> {
    Latch start(3);
    Latch finish(3);
    spawn(server(start, finish));
    spawn(client(start, finish));
    co_await start.arrive_and_wait();
    co_await finish.arrive_and_wait();
    co_return;
};

auto main() -> int {
    block_on(test());
}
