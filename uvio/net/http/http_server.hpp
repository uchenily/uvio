#pragma once

#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/tcp_listener.hpp"

namespace uvio::net::http {

class HttpServer {
public:
    HttpServer(std::string_view host, int port)
        : host_{host}
        , port_{port} {}

public:
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
    auto handle_http(TcpStream stream) -> Task<void> {
        HttpFramed http_framed{std::move(stream)};

        auto req = co_await http_framed.read_request();
        if (!req) {
            console.info("client closed");
            co_return;
        }

        auto request = std::move(req.value());
        LOG_DEBUG("request.body: {}", request.body);

        HttpResponse resp{
            .http_code = 200,
            .body = "<h1>Hello</h1>",
        };

        if (auto res = co_await http_framed.write_response(resp); !res) {
            console.error("{}", res.error().message());
            co_return;
        }
    }

private:
    std::string host_;
    int         port_;
    TcpStream   stream_{nullptr};
};

} // namespace uvio::net::http