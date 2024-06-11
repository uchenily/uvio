#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include <memory>

namespace uvio::io {

template <typename IO>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
    }
class BufReader : public detail::ImplBufRead<BufReader<IO>> {
    friend class detail::ImplBufRead<BufReader<IO>>;

public:
    BufReader() = default;

    BufReader(IO &&io, std::size_t size)
        : io_{std::move(io)}
        , r_stream_{size} {}

    BufReader(BufReader &&other) noexcept
        : io_{std::move(other.io_)}
        , r_stream_{std::move(other.r_stream_)} {}

    auto operator=(BufReader &&other) noexcept -> BufReader & {
        if (std::addressof(other) != this) [[likely]] {
            io_ = std::move(other.io_);
            r_stream_ = std::move(other.r_stream_);
        }
        return *this;
    }

    // No copy
    BufReader(const BufReader &other) = delete;
    auto operator=(const BufReader &other) = delete;

public:
    auto inner() noexcept -> IO & {
        return io_;
    }

    auto into_inner() noexcept -> IO {
        r_stream_.disable();
        return std::move(io_);
    }

private:
    IO                   io_;
    detail::StreamBuffer r_stream_;
};
} // namespace uvio::io
