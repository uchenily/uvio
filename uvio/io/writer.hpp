#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_writer.hpp"

namespace uvio::io {

template <class IO>
    requires requires(IO io, std::span<const char> buf) {
        { io.write(buf) };
    }
class BufWriter : public detail::ImplBufWrite<BufWriter<IO>> {
    friend class detail::ImplBufWrite<BufWriter<IO>>;

public:
    BufWriter(IO &&io, std::size_t size)
        : io_{std::move(io)}
        , w_stream_{size} {}

    BufWriter(BufWriter &&other) noexcept
        : io_{std::move(other.io_)}
        , w_stream_{std::move(other.w_stream_)} {}

    auto operator=(BufWriter &&other) noexcept -> BufWriter & {
        if (std::addressof(other) != this) [[likely]] {
            io_ = std::move(other.io_);
            w_stream_ = std::move(other.w_stream_);
        }
        return *this;
    }

    // No copy
    BufWriter(const BufWriter &other) = delete;
    auto operator=(const BufWriter &other) = delete;

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
    IO                   io_;
    detail::StreamBuffer w_stream_;
};

} // namespace uvio::io
