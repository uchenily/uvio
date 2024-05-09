#include "uvio/core.hpp"
#include "uvio/net.hpp"
#include "uvio/sync.hpp"

using namespace uvio;
using namespace uvio::sync;
using namespace uvio::net;

auto main() -> int {

    auto server = []() -> Task<> {
        std::array<char, 64> buf{};

        auto listener = TcpListener();
        listener.bind("localhost", 12345);
        // start.count_down();

        auto stream = co_await listener.accept();
        auto nread = co_await stream.read(buf);
        // EXPECT_EQ(nread, 12);
        // EXPECT_STREQ(buf.data(), "test message");
        co_await stream.write(buf.data());

        // finish.count_down();
    };

    auto client = []() -> Task<> {
        std::array<char, 64> buf{};

        // co_await start.arrive_and_wait();
        auto stream = co_await TcpStream::connect("localhost", 12345);
        co_await stream.write("test message");
        auto nread = co_await stream.read(buf);
        // EXPECT_EQ(nread, 12);
        // EXPECT_STREQ(buf.data(), "test message");

        // finish.count_down();
    };

    auto test = [&]() -> Task<> {
        Latch start(3);
        Latch finish(3);
        spawn(server());
        spawn(client());
        // co_await start.arrive_and_wait();
        // co_await finish.arrive_and_wait();
        co_return;
    };

    block_on(test());
}