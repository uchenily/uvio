#pragma once

#include "uvio/core.hpp"
#include "uvio/log.hpp"

namespace uvio::codec {

using namespace uvio::log;
using namespace uvio;

template <typename Derived, typename Reader, typename Writer>
class Codec {
public:
    auto Decode(Reader &reader) -> Task<std::string> {
        co_return co_await static_cast<Derived *>(this)->decode(reader);
    }

    auto Encode(const std::span<const char> message, Writer &writer)
        -> Task<void> {
        co_await static_cast<Derived *>(this)->encode(message, writer);
    }
};

template <typename Reader, typename Writer>
class LengthDelimitedCodec
    : public Codec<LengthDelimitedCodec<Reader, Writer>, Reader, Writer> {
private:
    friend class Codec<LengthDelimitedCodec<Reader, Writer>, Reader, Writer>;

    auto decode(Reader &reader) -> Task<std::string> {
        std::array<unsigned char, 4> msg_len{};

        auto ret = co_await reader.read_exact(
            {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        if (!ret) {
            console.error("decode error: {}", ret.error().message());
            co_return std::string{};
        }

        auto length = msg_len[0] << 24 | msg_len[1] << 16 | msg_len[2] << 8
                      | msg_len[3];

        std::string message(length, 0);
        ret = co_await reader.read_exact(message);
        if (!ret) {
            console.error("decode error: {}", ret.error().message());
            co_return std::string{};
        }
        co_return message;
    }

    auto encode(const std::span<const char> message, Writer &writer)
        -> Task<void> {
        std::array<unsigned char, 4> msg_len{};
        uint32_t                     length = message.size();

        msg_len[3] = length & 0xFF;
        msg_len[2] = (length >> 8) & 0xFF;
        msg_len[1] = (length >> 16) & 0xFF;
        msg_len[0] = (length >> 24) & 0xFF;

        auto ret = co_await writer.write_all(
            {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        if (!ret) {
            console.error("encode error: {}", ret.error().message());
            co_return;
        }

        ret = co_await writer.write_all(message);
        if (!ret) {
            console.error("encode error: {}", ret.error().message());
            co_return;
        }
        co_await writer.flush();
    }
};

} // namespace uvio::codec
