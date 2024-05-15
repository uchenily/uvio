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
        // When buf is large enough, it's not necessory for additional copy from
        // StreamBuffer to buf
        if (r_stream_.capacity() < buf.size_bytes()) {
            auto len = r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret = co_await io_.read(buf);
            if (ret) {
                ret.value() += len;
            }
            co_return ret;
        }

        if (!r_stream_.empty()) {
            co_return r_stream_.write_to(buf);
        }

        std::size_t ret{};
        while (true) {
            // TODO(x)
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

    [[REMEMBER_CO_AWAIT]]
    auto write(std::span<const char> buf) {
        if (w_stream_.w_remaining() >= buf.size_bytes()) {
            co_return w_stream_.read_from(buf);
        }

        std::size_t written_bytes{};
        std::size_t ret{};
        std::size_t ret2{};
        do {
            if (w_stream_.r_remaining() > 0) {
                ret = co_await io_.write(w_stream_.r_slice());
                if (ret < 0) {
                    co_return ret;
                }

                ret2 = co_await io_.write(buf);
                if (ret2 < 0) {
                    co_return ret2;
                }

                ret += ret2;
            } else {
                ret = co_await io_.write(buf);
                if (ret < 0) {
                    co_return ret;
                }
            }

            auto len = std::min(ret, w_stream_.r_remaining());
            w_stream_.r_increase(len);
            // TODO(x): optimize
            w_stream_.reset_data();
            ret -= len;
            written_bytes += ret;
            buf = buf.subspan(ret, buf.size_bytes() - ret);
            break;
        } while (w_stream_.w_remaining() < buf.size_bytes());

        written_bytes += w_stream_.read_from(buf);
        co_return written_bytes;
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
