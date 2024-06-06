#include "uvio/debug.hpp"
#include "uvio/net/http.hpp"

using namespace uvio::net;
using namespace uvio::net::http;

auto main() -> int {
    http::HttpServer server{"0.0.0.0", 8000};
    server.set_handler("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.set_handler("/test", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\ntest route", req.method, req.uri);
    });
    server.run();
}
