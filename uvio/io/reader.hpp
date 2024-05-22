#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include <memory>

namespace uvio::io {

template <typename IO, int RBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
    }
class BufReader : public detail::ImplBufRead<IO> {
    friend class detail::ImplBufRead<BufReader<IO, RBUF_SIZE>>;

public:
    BufReader(IO &&io)
        : io_{std::move(io)} {}

    BufReader(BufReader &&other) noexcept
        : io_{std::move(other.io_)}
        , r_stream_{std::move(other.r_stream_)} {}

    auto operator=(BufReader &&other) noexcept -> BufReader & {
        if (std::addressof(other) != this) {
            io_ = std::move(other.io_);
            r_stream_ = std::move(other.r_stream_);
        }
        return *this;
    }

public:
    auto inner() noexcept -> IO & {
        return io_;
    }

    auto into_inner() noexcept -> IO {
        r_stream_.disable();
        return std::move(io_);
    }

private:
    IO                              io_;
    detail::StreamBuffer<RBUF_SIZE> r_stream_;
};
} // namespace uvio::io
