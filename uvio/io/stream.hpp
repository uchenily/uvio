#pragma once

#include "uvio/common/result.hpp"
#include "uvio/coroutine/task.hpp"
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
    auto read(std::span<char> buf) -> Task<Result<std::size_t>> {
        // When buf is large enough, it's not necessory for additional copy from
        // StreamBuffer to buf
        if (r_stream_.capacity() < buf.size_bytes()) {
            auto len = r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret = co_await io_.read(buf);
            if (!ret) {
                co_return ret;
            }
            ret.value() += len;
            co_return ret;
        }

        if (!r_stream_.empty()) {
            co_return r_stream_.write_to(buf);
        }

        while (true) {
            // TODO(x)
            // if (r_stream_.r_remaining() + r_stream_.w_remaining()
            if (r_stream_.w_remaining() < static_cast<int>(buf.size_bytes())) {
                r_stream_.reset_data();
            }

            auto ret = co_await io_.read(r_stream_.w_slice());
            if (!ret) {
                co_return ret;
            }
            r_stream_.w_increase(ret.value());

            if (!r_stream_.empty() || ret.value() == 0) {
                co_return r_stream_.write_to(buf);
            }
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_exact(std::span<char> buf) -> Task<Result<void>> {
        if (r_stream_.capacity() < buf.size_bytes()) {
            auto len = r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret = co_await io_.read_exact(buf);
            if (!ret) {
                co_return ret;
            }
            co_return Result<void>{};
        }

        auto exact_bytes = static_cast<int>(buf.size_bytes());

        if (r_stream_.r_remaining() >= exact_bytes) {
            r_stream_.write_to(buf);
            co_return Result<void>{};
        }

        while (true) {
            if (r_stream_.w_remaining() < exact_bytes) {
                r_stream_.reset_data();
            }

            auto ret = co_await io_.read(r_stream_.w_slice());
            if (!ret) {
                co_return unexpected{ret.error()};
            }
            r_stream_.w_increase(ret.value());

            if (r_stream_.r_remaining() >= exact_bytes) {
                r_stream_.write_to(buf);
                co_return Result<void>{};
            }
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_until(std::string &buf, std::string_view end_flag)
        -> Task<Result<std::size_t>> {
        Result<std::size_t> ret;
        while (true) {
            if (auto slice = r_stream_.find_flag_and_return_slice(end_flag);
                !slice.empty()) {
                buf.append(slice.begin(), slice.end());
                r_stream_.r_increase(slice.size_bytes());
                break;
            } else {
                auto readable = r_stream_.r_slice();
                buf.append(readable.begin(), readable.end());
                r_stream_.reset_pos();
            }

            ret = co_await io_.read(r_stream_.w_slice());
            if (!ret) {
                co_return unexpected{ret.error()};
            }
            // if (ret.value() == 0) {
            //     break;
            // }
            r_stream_.w_increase(ret.value());
        }

        co_return buf.size();
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_line(std::string &buf) -> Task<Result<std::size_t>> {
        return read_until(buf, "\n");
    }

    [[REMEMBER_CO_AWAIT]]
    auto write(std::span<const char> buf) -> Task<Result<std::size_t>> {
        if (w_stream_.w_remaining() >= static_cast<int>(buf.size())) {
            co_return w_stream_.read_from(buf);
        }

        Result<std::size_t> written_bytes{};
        if (w_stream_.r_remaining() > 0) {
            auto ret = co_await io_.write(w_stream_.r_slice());
            if (!ret) {
                co_return ret;
            }
            written_bytes.value() += w_stream_.r_remaining();
            w_stream_.r_increase(w_stream_.r_remaining());
            w_stream_.reset_data();

            auto ret2 = co_await io_.write(buf);
            if (!ret2) {
                co_return ret2;
            }
            written_bytes.value() += buf.size();

        } else {
            auto ret = co_await io_.write(buf);
            if (!ret) {
                co_return ret;
            }
            written_bytes.value() += buf.size();
        }

        co_return written_bytes;
    }

    [[REMEMBER_CO_AWAIT]]
    auto flush() -> Task<Result<void>> {
        while (!w_stream_.r_slice().empty()) {
            auto ret = co_await io_.write(w_stream_.r_slice());
            if (!ret) {
                co_return unexpected{ret.error()};
            }

            w_stream_.r_increase(w_stream_.r_remaining());
        }
        w_stream_.reset_pos();
        co_return Result<void>{};
    }

    [[REMEMBER_CO_AWAIT]]
    auto write_all(std::span<const char> buf) -> Task<Result<void>> {
        if (auto ret = co_await write(buf); !ret) {
            co_return unexpected{ret.error()};
        }
        if (auto ret = co_await flush(); !ret) {
            co_return unexpected{ret.error()};
        }
        co_return Result<void>{};
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
