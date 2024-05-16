#pragma once

#include "uvio/coroutine/task.hpp"
#include "uvio/io/buffer.hpp"
#include "uvio/macros.hpp"

namespace uvio::io {

template <typename IO, int RBUF_SIZE, int WBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
        { io.write(std::string_view{buf.data(), buf.size()}) };
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
    auto read(std::span<char> buf) -> Task<ssize_t> {
        // When buf is large enough, it's not necessory for additional copy from
        // StreamBuffer to buf
        if (r_stream_.capacity() < buf.size_bytes()) {
            auto len = r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret = co_await io_.read(buf);
            if (ret < 0) {
                co_return ret;
            }
            ret += len;
            co_return ret;
        }

        if (!r_stream_.empty()) {
            co_return r_stream_.write_to(buf);
        }

        ssize_t ret{};
        while (true) {
            LOG_DEBUG("in read");
            // TODO(x)
            // if (r_stream_.r_remaining() + r_stream_.w_remaining()
            if (r_stream_.w_remaining() < static_cast<int>(buf.size_bytes())) {
                r_stream_.reset_data();
            }

            LOG_DEBUG("w_remaining: {}", r_stream_.w_remaining());
            ret = co_await io_.read(r_stream_.w_slice());
            LOG_DEBUG("io_.read() ret: {}", ret);
            if (ret < 0) {
                LOG_ERROR(uv_err_name(static_cast<int>(ret)));
                co_return ret;
            }
            r_stream_.w_increase(ret);

            if (!r_stream_.empty() || ret == 0) {
                co_return r_stream_.write_to(buf);
            }
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto write(std::string_view buf) -> Task<bool> {
        if (w_stream_.w_remaining() >= static_cast<int>(buf.size())) {
            co_return w_stream_.read_from(buf);
        }

        std::size_t written_bytes{};
        bool        ok{};
        bool        ok2{};
        if (w_stream_.r_remaining() > 0) {
            // ok = co_await io_.write(w_stream_.r_slice());
            ok = co_await io_.write({w_stream_.r_begin(), w_stream_.r_end()});
            if (!ok) {
                co_return ok;
            }
            written_bytes += w_stream_.r_remaining();
            w_stream_.r_increase(w_stream_.r_remaining());
            w_stream_.reset_data();

            ok2 = co_await io_.write(buf);
            if (!ok2) {
                co_return ok2;
            }
            written_bytes += buf.size();

        } else {
            ok = co_await io_.write(buf);
            if (!ok) {
                co_return ok;
            }
            written_bytes += buf.size();
        }

        // co_return written_bytes;
        co_return true;
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
