#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include "uvio/io/impl/buf_writer.hpp"

namespace uvio::io {

template <typename IO, int RBUF_SIZE, int WBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
        { io.write(buf) };
    }
class BufStream
    : public detail::ImplBufRead<BufStream<IO, RBUF_SIZE, WBUF_SIZE>>,
      public detail::ImplBufWrite<BufStream<IO, RBUF_SIZE, WBUF_SIZE>> {
    friend class detail::ImplBufRead<BufStream<IO, RBUF_SIZE, WBUF_SIZE>>;
    friend class detail::ImplBufWrite<BufStream<IO, RBUF_SIZE, WBUF_SIZE>>;

public:
    BufStream(IO &&io)
        : io_{std::move(io)} {}

    BufStream(BufStream &&other) noexcept
        : io_{std::move(other.io_)}
        , r_stream_{std::move(other.r_stream_)}
        , w_stream_{std::move(other.w_stream)} {}

    auto operator=(BufStream &&other) noexcept -> BufStream & {
        if (std::addressof(other) != this) {
            io_ = std::move(other.io_);
            r_stream_ = std::move(other.r_stream_);
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
        r_stream_.disable();
        w_stream_.disable();
        return std::move(io_);
    }

private:
    IO                              io_;
    detail::StreamBuffer<RBUF_SIZE> r_stream_;
    detail::StreamBuffer<WBUF_SIZE> w_stream_;
};

} // namespace uvio::io
