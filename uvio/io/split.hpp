#pragma once

#include "uvio/io/buffer.hpp"
#include "uvio/io/impl/buf_reader.hpp"
#include "uvio/io/impl/buf_writer.hpp"
#include <memory>

namespace uvio::io {

template <typename IO, int RBUF_SIZE>
    requires requires(IO io, std::span<char> buf) {
        { io.read(buf) };
    }
class OwnedReadHalf : public detail::ImplBufRead<OwnedReadHalf<IO, RBUF_SIZE>> {
    friend class detail::ImplBufRead<OwnedReadHalf<IO, RBUF_SIZE>>;

public:
    OwnedReadHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto read(std::span<char> buf) {
        return stream_->read(buf);
    }

private:
    std::shared_ptr<IO>             stream_;
    IO                             &io_{*stream_.get()};
    detail::StreamBuffer<RBUF_SIZE> r_stream_;
};

template <typename IO, int WBUF_SIZE>
    requires requires(IO io, std::span<const char> buf) {
        { io.write(buf) };
    }
class OwnedWriteHalf
    : public detail::ImplBufWrite<OwnedWriteHalf<IO, WBUF_SIZE>> {
    friend class detail::ImplBufWrite<OwnedWriteHalf<IO, WBUF_SIZE>>;

public:
    OwnedWriteHalf(std::shared_ptr<IO> stream)
        : stream_{std::move(stream)} {}

public:
    auto write(std::span<const char> buf) {
        return stream_->write(buf);
    }

private:
    std::shared_ptr<IO>             stream_;
    IO                             &io_{*stream_.get()};
    detail::StreamBuffer<WBUF_SIZE> w_stream_;
};

} // namespace uvio::io
