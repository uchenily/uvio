#include "uvio/core.hpp"
#include "uvio/net.hpp"
#include "uvio/sync.hpp"
#include "uvio/time.hpp"

#include "gtest/gtest.h"

using namespace uvio;
using namespace uvio::net;
using namespace uvio::time;
using namespace uvio::sync;

TEST(TestTcpListener, ListenAndAccept) {

    auto server = [](Latch &start, Latch &finish) -> Task<> {
        std::array<char, 64> buf{};

        auto listener = TcpListener();
        listener.bind("localhost", 12345);
        start.count_down();

        auto stream = co_await listener.accept();
        auto nread = co_await stream.read(buf);
        EXPECT_EQ(nread, 12);
        EXPECT_STREQ(buf.data(), "test message");
        co_await stream.write(buf.data());

        finish.count_down();
    };

    auto client = [](Latch &start, Latch &finish) -> Task<> {
        std::array<char, 64> buf{};

        co_await start.arrive_and_wait();
        auto stream = co_await TcpStream::connect("localhost", 12345);
        co_await stream.write("test message");
        auto nread = co_await stream.read(buf);
        EXPECT_EQ(nread, 12);
        EXPECT_STREQ(buf.data(), "test message");

        finish.count_down();
    };

    auto test = [&]() -> Task<> {
        Latch start(3);
        Latch finish(3);
        spawn(server(start, finish));
        spawn(client(start, finish));
        co_await start.arrive_and_wait();
        co_await finish.arrive_and_wait();
    };

    block_on(test());
}
