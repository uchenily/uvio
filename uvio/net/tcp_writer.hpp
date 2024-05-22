#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_writer.hpp"

namespace uvio::io {

template <class IO, int WBUF_SIZE>
    requires requires(IO io, std::span<const char> buf) {
        { io.write(buf) };
    }
class TcpBufWriter : public detail::ImplBufWrite<TcpBufWriter<IO, WBUF_SIZE>> {
    friend class detail::ImplBufWrite<TcpBufWriter<IO, WBUF_SIZE>>;

public:
    TcpBufWriter(IO &&io)
        : io_{std::move(io)} {}

    TcpBufWriter(TcpBufWriter &&other) noexcept
        : io_{std::move(other.io_)}
        , w_stream_{std::move(other.w_stream_)} {}

    auto operator=(TcpBufWriter &&other) noexcept -> TcpBufWriter & {
        if (std::addressof(other) != this) {
            io_ = std::move(other.io_);
            w_stream_ = std::move(other.w_stream_);
        }
        return *this;
    }

public:
    [[nodiscard]]
    auto inner() noexcept -> IO & {
        return io_;
    }

    [[nodiscard]]
    auto into_inner() noexcept -> IO {
        w_stream_.disable();
        return std::move(io_);
    }

private:
    IO                              io_;
    detail::StreamBuffer<WBUF_SIZE> w_stream_;
};

} // namespace uvio::io
