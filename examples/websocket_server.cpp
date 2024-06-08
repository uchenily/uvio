#include "uvio/net/http.hpp"
#include "uvio/net/websocket.hpp"

using namespace uvio::net::http;
using namespace uvio::net;

auto process_message(websocket::WebsocketFramed &channel) -> Task<> {
    // Ignore errors
    for (int i = 0; i < 64; i++) {
        auto msg = (co_await channel.recv()).value();
        LOG_INFO("Received: `{}`", std::string_view{msg.data(), msg.size()});
        co_await channel.send(msg);
    }
    co_await channel.close();
    co_return;
}

auto main() -> int {
    websocket::WebsocketServer server{"0.0.0.0", 8000};
    server.add_route("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.handle_message(process_message);
    server.run();
}
