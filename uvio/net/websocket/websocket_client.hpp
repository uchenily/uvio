#pragma once

#include "uvio/common/base64.hpp"
#include "uvio/core.hpp"
#include "uvio/crypto/secure_hash.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/tcp_listener.hpp"
#include "uvio/net/websocket/websocket_frame.hpp"

namespace uvio::net::websocket {

class WebsocketClient {
    using WebsocketHandlerFunc
        = std::function<Task<>(WebsocketFramed &websocket_framed)>;

public:
    WebsocketClient(std::string_view host, int port)
        : host_{host}
        , port_{port} {}

public:
    auto handle_message(WebsocketHandlerFunc &&func) {
        websocket_handler_ = std::move(func);
    }

    auto run() {
        block_on([this]() -> Task<> {
            console.info("Connecting to {}:{} ...", this->host_, this->port_);
            auto has_stream
                = co_await TcpStream::connect(this->host_, this->port_);
            if (!has_stream) {
                LOG_ERROR("{}", has_stream.error().message());
            }
            auto stream = std::move(has_stream.value());
            spawn(this->handle_websocket(std::move(stream)));
        }());
    }

private:
    auto handle_websocket(TcpStream stream) -> Task<> {
        WebsocketFramed websocket_framed{std::move(stream)};
        websocket_framed.client_side();

        // GET␣/␣HTTP/1.1\r\nHost:␣localhost:8000\r\nUpgrade:␣websocket\r\nConnection:␣Upgrade\r\nSec-WebSocket-Key:␣8AVhXO1zSlflZw1LfElVnw==\r\nSec-WebSocket-Version:␣13\r\nSec-WebSocket-Extensions:␣permessage-deflate;␣client_max_window_bits\r\nUser-Agent:␣Python/3.12␣websockets/12.0\r\n\r\n
        http::HttpRequest req{
            .method = "GET",
            .uri = "/",
        };
        req.headers.add("Upgrade", "websocket");
        req.headers.add("Connection", "Upgrade");
        // req.headers.add("Sec-WebSocket-Key", security_key);
        req.headers.add("Sec-WebSocket-Key", "8AVhXO1zSlflZw1LfElVnw==");
        req.headers.add("Sec-WebSocket-Version", "13");
        // req.headers.add("Sec-WebSocket-Extensions", "permessage-deflate;
        // client_max_window_bits");
        req.headers.add("User-Agent", "UVIO/0.0.1");

        if (auto ok = co_await websocket_framed.send_request(req); !ok) {
            console.info("handshake failed");
            co_return;
        }
        if (auto ok = co_await websocket_framed.recv_response(); !ok) {
            console.info("handshake failed");
            co_return;
        }

        if (websocket_handler_) {
            co_await websocket_handler_(websocket_framed);
        }
        co_return;
    }

private:
    std::string          host_;
    int                  port_;
    TcpStream            stream_{nullptr};
    WebsocketHandlerFunc websocket_handler_;
};

} // namespace uvio::net::websocket
