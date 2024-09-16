#pragma once

#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/http/http_frame.hpp"
#include "uvio/net/http/http_protocol.hpp"
#include "uvio/net/tcp_listener.hpp"

#include <regex>
#include <unordered_map>

namespace uvio::net::http {

class HttpServer {
    using HandlerCoro
        = std::function<Task<>(const HttpRequest &req, HttpResponse &resp)>;

public:
    HttpServer(std::string_view host, int port)
        : host_{host}
        , port_{port} {}

public:
    auto add_route(std::string_view uri, HandlerCoro &&coro) {
        map_handles_[uri] = std::move(coro);
    }

    auto run() {
        block_on([this]() -> Task<> {
            auto listener = TcpListener();
            listener.bind(this->host_, this->port_);
            console.info("Listening on {}:{} ...", this->host_, this->port_);
            while (true) {
                auto stream = (co_await listener.accept()).value();
                spawn(this->handle_http(std::move(stream)));
            }
        }());
    }

private:
    auto handle_http(TcpStream stream) -> Task<> {
        HttpFramed http_framed{std::move(stream)};

        auto req = co_await http_framed.read_request();
        if (!req) {
            console.info("client closed");
            co_return;
        }

        auto request = std::move(req.value());
        LOG_DEBUG("request uri: {} body: {}", request.uri, request.body);

        HttpResponse resp;
        std::smatch  match;
        for (auto &route : map_handles_) {
            if (std::regex_match(request.uri,
                                 match,
                                 std::regex{std::string{route.first}})) {
                co_await route.second(request, resp);
                resp.status_code = 200;
                resp.status_text = "OK";
                LOG_DEBUG("uri `{}` matched pattern `{}`",
                          request.uri,
                          route.first);
                break;
            } else {
            }
        }

        if (match.empty()) {
            resp.body = "Page not found";
            resp.status_code = 404;
            resp.status_text = "Not Found";
        }

        // if (auto it = map_handles_.find(request.uri);
        //     it != map_handles_.end()) {
        //     co_await it->second(request, resp);
        //     resp.status_code = 200;
        //     resp.status_text = "OK";
        // } else {
        //     // TODO(x)
        //     resp.body = "Page not found";
        //     resp.status_code = 404;
        //     resp.status_text = "Not Found";
        // }

        // HttpResponse resp{
        //     .http_code = 200,
        //     .body = "<h1>Hello</h1>",
        // };

        if (auto res = co_await http_framed.write_response(resp); !res) {
            console.error("{}", res.error().message());
            co_return;
        }
    }

private:
    std::string host_;
    int         port_;
    TcpStream   stream_{nullptr};

    std::unordered_map<std::string_view, HandlerCoro> map_handles_;
};

} // namespace uvio::net::http
