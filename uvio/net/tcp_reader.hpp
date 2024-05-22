#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"

namespace uvio::io {

template <typename IO, int RBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
    }
// class TcpBufReader : public detail::ImplBufRead<IO> { // wrong
class TcpBufReader : public detail::ImplBufRead<TcpBufReader<IO, RBUF_SIZE>> {
    friend class detail::ImplBufRead<TcpBufReader>;

public:
    TcpBufReader(IO &&io)
        : io_{std::move(io)} {}

    TcpBufReader(TcpBufReader &&other) noexcept
        : io_{std::move(other.io_)}
        , r_stream_{std::move(other.r_stream_)} {}

    auto operator=(TcpBufReader &&other) noexcept -> TcpBufReader & {
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
