#pragma once

#include "uvio/net/http/http_util.hpp"
#include "uvio/net/websocket/websocket_util.hpp"
#include <span>
#include <string>

namespace uvio::net::websocket {

struct HttpRequest {
    std::string      method;
    std::string      uri;
    http::HttpHeader headers;
    std::string      body;
};

struct HttpResponse {
    int              status_code;
    http::HttpHeader headers;
    std::string      body;
};

enum class Opcode {
    CONF = 0x00,
    TEXT = 0x01,
    BINARY = 0x02,
    CLOSE = 0x08,
    PING = 0x09,
    PONG = 0x0A,
};

struct WebsocketFrame {
    Opcode          opcode;
    std::span<char> message;

    auto payload_length() const {
        return message.size();
    }
};

} // namespace uvio::net::websocket
