#include "uvio/net/http.hpp"
#include "uvio/net/websocket.hpp"

using namespace uvio::net::http;
using namespace uvio::net;

auto main() -> int {
    websocket::WebsocketServer server{"0.0.0.0", 8000};
    server.add_route("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.handle_message([](websocket::WebsocketFramed &channel) -> Task<> {
        // Ignore errors
        auto message = (co_await channel.recv()).value();
        LOG_DEBUG("Received: {}",
                  std::string_view{message.data(), message.size()});
        co_await channel.send(message);

        auto message2 = (co_await channel.recv()).value();
        LOG_DEBUG("Received: {}",
                  std::string_view{message2.data(), message2.size()});
        co_await channel.send(message2);

        co_await channel.close();
        co_return;
    }

    );
    server.run();
}
