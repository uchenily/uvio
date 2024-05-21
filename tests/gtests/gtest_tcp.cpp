#include "uvio/core.hpp"
#include "uvio/net.hpp"
#include "uvio/sync.hpp"

#include "gtest/gtest.h"

using namespace uvio;
using namespace uvio::net;
using namespace uvio::sync;

TEST(TestTcpListener, ListenAndAccept) {
    Latch start(3);
    Latch finish(3);

    auto server = [&]() -> Task<> {
        std::array<char, 64> buf{};

        auto listener = TcpListener();
        listener.bind("127.0.0.1", 12345);
        start.count_down();

        auto stream = (co_await listener.accept()).value();
        auto nread = (co_await stream.read(buf)).value();
        // EXPECT_EQ(nread, 12);
        // EXPECT_STREQ(buf.data(), "test message");
        co_await stream.write(buf);

        finish.count_down();
    };

    auto client = [&]() -> Task<> {
        std::array<char, 64> buf{};

        co_await start.arrive_and_wait();
        auto stream = (co_await TcpStream::connect("127.0.0.1", 12345)).value();
        co_await stream.write("test message");
        auto nread = (co_await stream.read(buf)).value();
        // EXPECT_EQ(nread, 12);
        // EXPECT_STREQ(buf.data(), "test message");

        finish.count_down();
    };

    auto wait = [](Latch &start, Latch &finish) -> Task<> {
        co_await start.arrive_and_wait();
        co_await finish.arrive_and_wait();
    };

    spawn(server());
    spawn(client());
    block_on(wait(start, finish));
}
