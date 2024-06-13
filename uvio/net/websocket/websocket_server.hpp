#pragma once

#include "uvio/common/base64.hpp"
#include "uvio/core.hpp"
#include "uvio/crypto/secure_hash.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/tcp_listener.hpp"
#include "uvio/net/websocket/websocket_frame.hpp"

#include <unordered_map>

namespace uvio::net::websocket {

class WebsocketServer {
    using HandlerFunc = std::function<void(const http::HttpRequest &req,
                                           http::HttpResponse      &resp)>;

    using WebsocketHandlerFunc
        = std::function<Task<>(WebsocketFramed &websocket_framed)>;

public:
    WebsocketServer(std::string_view host, int port)
        : host_{host}
        , port_{port} {}

public:
    auto add_route(std::string_view uri, HandlerFunc &&func) {
        map_handles_[uri] = std::move(func);
    }

    auto handle_message(WebsocketHandlerFunc &&func) {
        websocket_handler_ = std::move(func);
    }

    auto run() {
        block_on([this]() -> Task<> {
            auto listener = TcpListener();
            listener.bind(this->host_, this->port_);
            console.info("Listening on {}:{} ...", this->host_, this->port_);
            while (true) {
                auto stream = (co_await listener.accept()).value();
                spawn(this->handle_websocket(std::move(stream)));
            }
        }());
    }

private:
    auto handle_websocket(TcpStream stream) -> Task<void> {
        WebsocketFramed websocket_framed{std::move(stream)};

        auto req = co_await websocket_framed.recv_request();
        if (!req) {
            console.info("client closed");
            co_return;
        }

        auto request = std::move(req.value());
        LOG_DEBUG("request uri: {} body: {}", request.uri, request.body);

        http::HttpResponse resp;
        if (request.headers.find("Upgrade")) {
            auto has_security_key = request.headers.find("Sec-webSocket-Key");
            if (!has_security_key) {
                LOG_ERROR("No header: `Sec-WebSocket-Key`");
                co_return;
            }
            auto security_key = std::move(has_security_key.value());
            security_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

            auto hash = secure_hash::sha1(security_key);
            auto base64_hash = base64::encode(hash);

            resp.headers.add("Upgrade", "websocket");
            resp.headers.add("Connection", "Upgrade");
            resp.headers.add("Sec-WebSocket-Version", "13");
            resp.headers.add("Sec-WebSocket-Accept", base64_hash);
            co_await websocket_framed.send_response(resp);

            if (websocket_handler_) {
                // TODO(x): 创建两个协程 (参照:
                // https://github.com/python-websockets/websockets/blob/12.0/src/websockets/legacy/protocol.py)
                // data_transfer(): 处理数据传输
                // keepalive_ping(): 处理ping/pong帧
                co_await websocket_handler_(websocket_framed);
            }
            co_return;
        }

        if (auto it = map_handles_.find(request.uri);
            it != map_handles_.end()) {
            it->second(request, resp);
            resp.status_code = 200;
        } else {
            // TODO(x)
            resp.body = "Page not found";
            resp.status_code = 404;
        }

        if (auto res = co_await websocket_framed.send_response(resp); !res) {
            console.error("{}", res.error().message());
            co_return;
        }
    }

private:
    std::string host_;
    int         port_;
    TcpStream   stream_{nullptr};

    std::unordered_map<std::string_view, HandlerFunc> map_handles_;
    WebsocketHandlerFunc                              websocket_handler_;
};

} // namespace uvio::net::websocket
