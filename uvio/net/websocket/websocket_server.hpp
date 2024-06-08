#pragma once

#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/tcp_listener.hpp"
#include "uvio/net/websocket/websocket_frame.hpp"
#include "uvio/net/websocket/websocket_util.hpp"

#include <openssl/ssl.h>
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

        auto req = co_await websocket_framed.read_request();
        if (!req) {
            console.info("client closed");
            co_return;
        }

        auto request = std::move(req.value());
        LOG_DEBUG("request uri: {} body: {}", request.uri, request.body);

        http::HttpResponse resp;
        if (request.headers.find("Upgrade")) {
            auto security_key
                = request.headers.find("sec-websocket-key").value();
            security_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            std::array<unsigned char, 20> hash;
#if OPENSSL_VERSION_NUMBER <= 0x030000000L
            SHA_CTX sha1;
            SHA1_Init(&sha1);
            SHA1_Update(&sha1, security_key.data(), security_key.size());
            SHA1_Final(hash.data(), &sha1);
#else
            EVP_MD_CTX *sha1 = EVP_MD_CTX_new();
            EVP_DigestInit_ex(sha1, EVP_sha1(), nullptr);
            EVP_DigestUpdate(sha1, security_key.data(), security_key.size());
            EVP_DigestFinal_ex(sha1, hash.data(), nullptr);
            EVP_MD_CTX_free(sha1);
#endif
            auto solved_hash = base64_encode(hash.data(), hash.size());

            resp.headers.add("Upgrade", "websocket");
            resp.headers.add("Connection", "Upgrade");
            resp.headers.add("Sec-WebSocket-Version", "13");
            resp.headers.add("Sec-WebSocket-Accept", solved_hash);
            co_await websocket_framed.write_response(resp);

            if (websocket_handler_) {
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

        if (auto res = co_await websocket_framed.write_response(resp); !res) {
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
