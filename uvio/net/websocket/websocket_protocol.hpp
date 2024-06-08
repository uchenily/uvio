#pragma once

#include <span>

namespace uvio::net::websocket {

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
