#include "uvio/net/http.hpp"

using namespace uvio::net::http;

auto main() -> int {
    HttpServer server{"0.0.0.0", 8000};
    server.add_route(
        R"(^/$)",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<void> {
            resp.body = std::format("{} {}\r\nIndex Page", req.method, req.uri);
            co_return;
        });

    server.add_route(
        R"(^/about$)",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<void> {
            resp.body = std::format("{} {}\r\nAbout Page", req.method, req.uri);
            co_return;
        });

    server.add_route(
        R"(^/user/(\d+)$)",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<void> {
            // TODO(chen): auto user_id = req.get_param("userId");
            auto user_id = req.get_param(0);
            resp.body = std::format("User ID: {}\r\n", user_id);
            co_return;
        });

    server.add_route(
        R"(^/user/(\d+)/article/(\w+)$)",
        [](const HttpRequest &req, HttpResponse &resp) -> Task<void> {
            // TODO(chen):
            // auto user_id = req.get_param("userId");
            // auto article_id = req.get_param("articleId");
            auto user_id = req.get_param(0);
            auto article_id = req.get_param(1);
            resp.body = std::format("User ID: {} Article: {}\r\n",
                                    user_id,
                                    article_id);
            co_return;
        });
    server.run();
}
