#pragma once

#include "uvio/common/result.hpp"
#include "uvio/coroutine/task.hpp"
#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include "uvio/macros.hpp"

namespace uvio::io {

template <typename IO, int RBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
    }
class TcpBufReader {

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

public:
    [[REMEMBER_CO_AWAIT]]
    auto read(std::span<char> buf) -> Task<Result<std::size_t>> {
        // When buf is large enough, it's not necessory for additional copy
        // from StreamBuffer to buf
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
            // if (r_stream_.r_remaining() +
            // r_stream_.w_remaining()
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

private:
    IO                              io_;
    detail::StreamBuffer<RBUF_SIZE> r_stream_;
};
} // namespace uvio::io
