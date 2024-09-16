#include "uvio/net/http.hpp"

using namespace uvio::net::http;

auto main() -> int {
    HttpServer server{"0.0.0.0", 8000};
    server.add_route(
        "/",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<> {
            resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
            co_return;
        });

    server.add_route(
        "/test",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<void> {
            resp.body = std::format("{} {}\r\ntest route", req.method, req.uri);
            co_return;
        });

    server.add_route(R"(^/user/(\d+)$)",
                     [](const HttpRequest &req, HttpResponse &resp) -> Task<> {
                         auto user_id = req.uri.substr(
                             std::string_view{"/user/"}.length());
                         resp.body = std::format("User ID: {}\r\n", user_id);
                         co_return;
                     });
    server.run();
}
