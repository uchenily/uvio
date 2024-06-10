#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include "uvio/io/impl/buf_writer.hpp"

namespace uvio::io {

template <typename IO>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
        { io.write(buf) };
    }
class BufStream : public detail::ImplBufRead<BufStream<IO>>,
                  public detail::ImplBufWrite<BufStream<IO>> {
    friend class detail::ImplBufRead<BufStream<IO>>;
    friend class detail::ImplBufWrite<BufStream<IO>>;

public:
    BufStream(IO &&io, std::size_t rbuf_size, std::size_t wbuf_size)
        : io_{std::move(io)}
        , r_stream_{rbuf_size}
        , w_stream_{wbuf_size} {}

    BufStream(BufStream &&other) noexcept
        : io_{std::move(other.io_)}
        , r_stream_{std::move(other.r_stream_)}
        , w_stream_{std::move(other.w_stream)} {}

    auto operator=(BufStream &&other) noexcept -> BufStream & {
        if (std::addressof(other) != this) [[likely]] {
            io_ = std::move(other.io_);
            r_stream_ = std::move(other.r_stream_);
            w_stream_ = std::move(other.w_stream_);
        }
        return *this;
    }

    // No copy
    BufStream(const BufStream &other) = delete;
    auto operator=(const BufStream &other) = delete;

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
    IO                   io_;
    detail::StreamBuffer r_stream_;
    detail::StreamBuffer w_stream_;
};

} // namespace uvio::io
