#pragma once

#include "uvio/common/result.hpp"
#include "uvio/core.hpp"

namespace uvio::codec {

template <typename Derived>
class Codec {
public:
    template <typename Reader>
    auto Decode(Reader &reader) -> Task<Result<std::string>> {
        co_return co_await static_cast<Derived *>(this)->decode(reader);
    }

    template <typename Writer>
    auto Encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        co_return co_await static_cast<Derived *>(this)->encode(message,
                                                                writer);
    }
};

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

class LengthDelimitedCodec : public Codec<LengthDelimitedCodec> {
public:
    // friend class Codec<LengthDelimitedCodec<Reader, Writer>, Reader, Writer>;

    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<std::string>> {
        // std::array<unsigned char, 4> msg_len{};
        //
        // auto ret = co_await reader.read_exact(
        //     {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        // if (!ret) {
        //     co_return unexpected{ret.error()};
        // }
        //
        // auto length = msg_len[0] << 24 | msg_len[1] << 16 | msg_len[2] << 8
        //               | msg_len[3];

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
        // std::array<unsigned char, 4> msg_len{};
        // uint64_t                     length = message.size();
        //
        // msg_len[3] = length & 0xFF;
        // msg_len[2] = (length >> 8) & 0xFF;
        // msg_len[1] = (length >> 16) & 0xFF;
        // msg_len[0] = (length >> 24) & 0xFF;
        //
        // auto ret = co_await writer.write(
        //     {reinterpret_cast<char *>(msg_len.data()), msg_len.size()});
        // if (!ret) {
        //     co_return unexpected{ret.error()};
        // }

        uint64_t length = message.size();
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
    template <typename Reader>
    auto decode_length(Reader &reader) -> Task<Result<uint64_t>> {
        // Maximum number of bytes required to encode an uint64_t (7 * 10 > 64)
        static constexpr int max_varint_bytes = 10;

        std::array<char, 1> bytes{};
        uint64_t            value{};
        for (int i = 0; i < max_varint_bytes; ++i) {
            auto ret = co_await reader.read_exact(bytes);
            if (!ret) {
                co_return unexpected{ret.error()};
            }
            value |= static_cast<uint64_t>(bytes[0] & 0x7F) << (i * 7);
            if ((bytes[0] & 0x80) == 0) {
                co_return value;
            }
        }
        co_return unexpected{make_uvio_error(Error::Unclassified)};
    }

    template <typename Writer>
    auto encode_length(uint64_t value, Writer &writer) -> Task<Result<void>> {
        // Encodes value in varint format and writes the result by writer
        std::array<char, 1> bytes;
        do {
            bytes[0] = (value & 0x7F) | (((value >> 7) == 0) ? 0x00 : 0x80);
            if (auto ret = co_await writer.write(bytes); !ret) {
                co_return unexpected{ret.error()};
            }
            value >>= 7;
        } while (value != 0);
        // if (auto ret = co_await writer.flush(); !ret) {
        //     co_return ret;
        // }
        co_return Result<void>{};
    }
};

} // namespace uvio::codec
