#pragma once

#include "uvio/common/result.hpp"
#include "uvio/coroutine/task.hpp"
#include "uvio/io/buffer.hpp"
#include "uvio/macros.hpp"

namespace uvio::io {

template <class IO, int WBUF_SIZE>
    requires requires(IO io, std::span<const char> buf) {
        { io.write(buf) };
    }
class TcpBufWriter {
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

public:
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

private:
    IO                              io_;
    detail::StreamBuffer<WBUF_SIZE> w_stream_;
};

} // namespace uvio::io
