#include "uvio/net/http.hpp"
#include "uvio/net/websocket.hpp"

using namespace uvio::net::http;
using namespace uvio::net;

auto main() -> int {
    websocket::WebsocketServer server{"0.0.0.0", 8000};
    server.set_handler("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.run();
}
