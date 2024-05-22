#pragma once

#include <memory>
#include <span>

namespace uvio::io {

template <typename IO>
class OwnedReadHalf {
public:
    OwnedReadHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto read(std::span<char> buf) {
        return stream_->read(buf);
    }

private:
    std::shared_ptr<IO> stream_;
};

template <typename IO>
class OwnedWriteHalf {
public:
    OwnedWriteHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto write(std::span<const char> buf) {
        return stream_->write(buf);
    }

private:
    std::shared_ptr<IO> stream_;
};

} // namespace uvio::io
