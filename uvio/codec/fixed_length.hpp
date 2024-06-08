#pragma once
#include "uvio/codec/base.hpp"

namespace uvio::codec {
class FixedLength32Codec : public Codec<FixedLength32Codec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<std::string>> {
        auto has_length = co_await decode_length(reader);
        if (!has_length) {
            co_return unexpected{has_length.error()};
        }
        auto length = has_length.value();

        std::string message(length, 0);
        if (auto ret = co_await reader.read_exact(message); !ret) {
            co_return unexpected{ret.error()};
        }
        co_return message;
    }

    template <typename Writer>
    auto encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        uint32_t length = message.size();
        if (auto ret = co_await encode_length(length, writer); !ret) {
            co_return ret;
        }
        if (auto ret = co_await writer.write(message); !ret) {
            co_return unexpected{ret.error()};
        }
        if (auto ret = co_await writer.flush(); !ret) {
            co_return ret;
        }
        co_return Result<void>{};
    }

private:
    // fixed 32bits
    static constexpr int fixed_length_bytes = 4;

private:
    template <typename Reader>
    auto decode_length(Reader &reader) -> Task<Result<uint32_t>> {
        std::array<char, 1> bytes{};
        uint32_t            value{};
        for (int i = 0; i < fixed_length_bytes; ++i) {
            auto ret = co_await reader.read_exact(bytes);
            if (!ret) {
                co_return unexpected{ret.error()};
            }
            value |= static_cast<uint32_t>(bytes[0] & 0xFF) << (i * 8);
        }
        co_return value;
    }

    template <typename Writer>
    auto encode_length(uint32_t value, Writer &writer) -> Task<Result<void>> {
        std::array<char, 1> bytes{};
        for (int i = 0; i < fixed_length_bytes; ++i) {
            bytes[0] = (value & 0xFF) | 0x00;
            if (auto ret = co_await writer.write(bytes); !ret) {
                co_return unexpected{ret.error()};
            }
            value >>= 8;
        }
        co_return Result<void>{};
    }
};

} // namespace uvio::codec
