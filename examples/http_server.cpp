#include "uvio/net/http.hpp"

using namespace uvio::net::http;

auto main() -> int {
    HttpServer server{"0.0.0.0", 8000};
    server.add_route("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.add_route("/test", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\ntest route", req.method, req.uri);
    });
    server.run();
}
