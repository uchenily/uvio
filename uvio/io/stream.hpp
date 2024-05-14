#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/macros.hpp"

namespace uvio::io {

template <typename IO, int RBUF_SIZE, int WBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
        { io.write(buf) };
    }
class BufStream {
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
    [[REMEMBER_CO_AWAIT]]
    auto read(std::span<char> buf) {
        if (!r_stream_.empty()) {
            co_return r_stream_.write_to(buf);
        }

        std::size_t ret{};
        while (true) {
            if (r_stream_.r_remaining() + r_stream_.w_remaining()
                < buf.size_bytes()) {
                r_stream_.reset_data();
            }

            ret = co_await io_.read(r_stream_.w_slice());
            if (ret < 0) {
                co_return ret;
            }
            r_stream_.w_increase(ret);

            if (!r_stream_.empty() || ret == 0) {
                co_return r_stream_.write_to(buf);
            }
        }
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
