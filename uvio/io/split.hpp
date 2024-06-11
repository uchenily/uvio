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

private:
    std::shared_ptr<IO> stream_;
};

template <typename IO>
auto reunite(OwnedReadHalf<IO>  &owned_read_half,
             OwnedWriteHalf<IO> &owned_write_half) -> Result<IO> {
    if (owned_read_half.stream_ != owned_write_half.stream_) {
        return std::unexpected{make_uvio_error(Error::ReuniteError)};
    }
    owned_write_half.stream_.reset();
    std::shared_ptr<IO> temp{nullptr};
    temp.swap(owned_read_half.stream_);
    return std::move(*temp);
}

} // namespace uvio::io
