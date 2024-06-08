#pragma once

#include "uvio/codec/base.hpp"
#include "uvio/net/endian.hpp"
#include "uvio/net/websocket/websocket_protocol.hpp"

namespace uvio::codec {

using namespace uvio::net::websocket;
class WebsocketCodec : public Codec<WebsocketCodec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<std::vector<char>>> {
        // https://github.com/python-websockets/websockets/blob/12.0/src/websockets/legacy/framing.py
        std::array<char, 2>          buf;
        std::array<char, 4>          mask_bits{};
        std::array<unsigned char, 8> buf8{};
        co_await reader.read_exact(buf);
        bool fin = buf[0] & 0b100000000;
        auto opcode = buf[0] & 0b00001111;
        ASSERT(fin);
        ASSERT(opcode == (int) Opcode::TEXT);

        // auto length = static_cast<size_t>(buf[1]); // error
        uint64_t length = (static_cast<unsigned char>(buf[1]) & 0b01111111);
        LOG_DEBUG("pre: {}", length);
        if (length == 126) {
            co_await reader.read_exact(buf);
            length = buf[1] << 8 | buf[0];
            length = net::byteorder::ntohs(length);
        } else if (length == 127) {
            co_await reader.read_exact(
                {reinterpret_cast<char *>(buf8.data()), buf8.size()});
            length = static_cast<uint64_t>(buf8[0])
                     | static_cast<uint64_t>(buf8[1]) << 8
                     | static_cast<uint64_t>(buf8[2]) << 16
                     | static_cast<uint64_t>(buf8[3]) << 24
                     | static_cast<uint64_t>(buf8[4]) << 32
                     | static_cast<uint64_t>(buf8[5]) << 40
                     | static_cast<uint64_t>(buf8[6]) << 48
                     | static_cast<uint64_t>(buf8[7]) << 56;
            length = net::byteorder::ntohll(length);
        }
        LOG_DEBUG("payload length: {}", length);

        if (!client_side_) {
            co_await reader.read_exact(mask_bits);
        }

        auto payload = std::vector<char>(length);
        co_await reader.read_exact(payload);
        if (!client_side_) {
            apply_mask(payload, mask_bits);
        }
        co_return payload;
    }

    template <typename Writer>
    auto encode(WebsocketFrame frame, Writer &writer) -> Task<Result<void>> {
        // https://github.com/python-websockets/websockets/blob/12.0/src/websockets/frames.py#L273
        std::array<unsigned char, 10> buf{};
        buf[0] = static_cast<char>(0b1000'0000)
                 | static_cast<char>(frame.opcode); // fin + opcode
        buf[1] = client_side_ ? static_cast<char>(0b1000'0000) : 0; // mask
        auto length = frame.payload_length();
        if (length < 126) {
            buf[1] |= length;
            co_await writer.write({reinterpret_cast<char *>(buf.data()), 2});
        } else if (length < 65536) {
            length = net::byteorder::htons(length);
            buf[1] |= 126;
            buf[2] = (length >> 0) & 0xFF;
            buf[3] = (length >> 8) & 0xFF;
            co_await writer.write({reinterpret_cast<char *>(buf.data()), 4});
        } else {
            length = net::byteorder::htonll(length);
            buf[1] |= 127;
            buf[2] = (length >> 0) & 0xFF;
            buf[3] = (length >> 8) & 0xFF;
            buf[4] = (length >> 16) & 0xFF;
            buf[5] = (length >> 24) & 0xFF;
            buf[6] = (length >> 32) & 0xFF;
            buf[7] = (length >> 40) & 0xFF;
            buf[8] = (length >> 48) & 0xFF;
            buf[9] = (length >> 56) & 0xFF;
            co_await writer.write(
                {reinterpret_cast<char *>(buf.data()), buf.size()});
        }

        if (client_side_) {
            // TODO(x)
            auto mask = std::array<char, 4>{1, 1, 1, 1};
            co_await writer.write(mask);
            apply_mask(frame.message, mask);
        }

        co_await writer.write(frame.message);
        co_await writer.flush();

        co_return Result<void>{};
    }

    auto client_side() {
        client_side_ = true;
    }

    auto apply_mask(std::span<char> data, std::array<char, 4> mask) -> void {
        auto length = data.size();
        for (auto i = 0u; i < (length & ~3); i += 4) {
            // integer promotions
            // cppcoreguidelines-narrowing-conversions
            // data[i + 0] ^= mask[0];
            // data[i + 1] ^= mask[1];
            // data[i + 2] ^= mask[2];
            // data[i + 3] ^= mask[3];

            data[i + 0] = static_cast<char>(data[i + 0] ^ mask[0]);
            data[i + 1] = static_cast<char>(data[i + 1] ^ mask[1]);
            data[i + 2] = static_cast<char>(data[i + 2] ^ mask[2]);
            data[i + 3] = static_cast<char>(data[i + 3] ^ mask[3]);
        }

        for (auto i = length & ~3; i < data.size(); ++i) {
            // data[i] ^= mask[i % 4];
            data[i] = static_cast<char>(data[i] ^ mask.at(i % 4));
        }
    }

private:
    bool client_side_{false};
};
} // namespace uvio::codec
