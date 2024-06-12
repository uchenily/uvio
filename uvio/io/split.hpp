#pragma once

#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include <memory>
#include <span>

namespace uvio::io {

template <typename IO>
class OwnedReadHalf {
public:
    OwnedReadHalf() = default;

    OwnedReadHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto read(std::span<char> buf) noexcept {
        return stream_->read(buf);
    }

    auto read_exact(std::span<char> buf) noexcept {
        return stream_->read_exact(buf);
    }

    auto inner() -> IO & {
        return *stream_.get();
    }

    auto take_inner() -> IO {
        std::shared_ptr<IO> temp{nullptr};
        temp.swap(this->stream_);
        return std::move(*temp);
    }

private:
    std::shared_ptr<IO> stream_;
};

template <typename IO>
class OwnedWriteHalf {
public:
    OwnedWriteHalf() = default;

    OwnedWriteHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto write(std::span<const char> buf) noexcept {
        return stream_->write(buf);
    }

    auto reset() {
        this->stream_.reset();
    }

    auto inner() -> IO & {
        return *stream_.get();
    }

private:
    std::shared_ptr<IO> stream_;
};

template <typename IO>
auto reunite(OwnedReadHalf<IO>  &owned_read_half,
             OwnedWriteHalf<IO> &owned_write_half) -> Result<IO> {
    if (std::addressof(owned_read_half.inner())
        != std::addressof(owned_write_half.inner())) {
        return unexpected{make_uvio_error(Error::ReuniteError)};
    }
    owned_write_half.reset();
    return owned_read_half.take_inner();
}

} // namespace uvio::io
