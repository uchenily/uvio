#pragma once

#include "uvio/core.hpp"

namespace uvio::codec {

using namespace uvio;

template <typename Derived, typename Reader, typename Writer>
class Codec {
public:
    auto Decode(Reader &reader) -> Task<Result<std::string>> {
        co_return co_await static_cast<Derived *>(this)->decode(reader);
    }

    auto Encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        co_return co_await static_cast<Derived *>(this)->encode(message,
                                                                writer);
    }
};

template <typename Reader, typename Writer>
class LengthDelimitedCodec
    : public Codec<LengthDelimitedCodec<Reader, Writer>, Reader, Writer> {
private:
    friend class Codec<LengthDelimitedCodec<Reader, Writer>, Reader, Writer>;

    auto decode(Reader &reader) -> Task<Result<std::string>> {
        std::array<unsigned char, 4> msg_len{};

        auto ret = co_await reader.read_exact(
            {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        if (!ret) {
            co_return unexpected{ret.error()};
        }

        auto length = msg_len[0] << 24 | msg_len[1] << 16 | msg_len[2] << 8
                      | msg_len[3];

        std::string message(length, 0);
        ret = co_await reader.read_exact(message);
        if (!ret) {
            co_return unexpected{ret.error()};
        }
        co_return message;
    }

    auto encode(const std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        std::array<unsigned char, 4> msg_len{};
        uint32_t                     length = message.size();

        msg_len[3] = length & 0xFF;
        msg_len[2] = (length >> 8) & 0xFF;
        msg_len[1] = (length >> 16) & 0xFF;
        msg_len[0] = (length >> 24) & 0xFF;

        auto ret = co_await writer.write(
            {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        if (!ret) {
            co_return unexpected{ret.error()};
        }

        ret = co_await writer.write(message);
        if (!ret) {
            co_return unexpected{ret.error()};
        }
        if (auto ret = co_await writer.flush(); !ret) {
            co_return ret;
        }
        co_return Result<void>{};
    }
};

} // namespace uvio::codec
