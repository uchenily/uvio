#include "uvio/net/websocket.hpp"

using namespace uvio::net;
using namespace uvio;

auto process_message(websocket::WebsocketFramed &channel) -> Task<> {
    // Ignore errors
    for (int i = 0; i < 64; i++) {
        auto msg = std::format("test message {} from client", i);
        co_await channel.send(msg);

        auto msg2 = (co_await channel.recv()).value();
        LOG_INFO("Received: `{}`", std::string_view{msg2.data(), msg2.size()});
    }
    co_await channel.close();
    co_return;
}

auto main() -> int {
    websocket::WebsocketClient client{"127.0.0.1", 8000};
    client.handle_message(process_message);
    client.run();
}
