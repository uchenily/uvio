#pragma once

#include "uvio/log.hpp"

#include <coroutine>
#include <memory>

#include <cassert>

#include <netinet/in.h>

#include "uv.h"

namespace uvio::net {

class TcpStream {
public:
    TcpStream(std::unique_ptr<uv_tcp_t> socket)
        : tcp_handle_{std::move(socket)} {}

    TcpStream(TcpStream &&other) noexcept
        : tcp_handle_{std::move(other.tcp_handle_)} {
        other.tcp_handle_ = nullptr;
    }
    auto operator=(TcpStream &&other) noexcept -> TcpStream & {
        this->tcp_handle_ = std::move(other.tcp_handle_);
        other.tcp_handle_ = nullptr;
        return *this;
    }

    TcpStream(const TcpStream &) = delete;
    auto operator=(const TcpStream &) = delete;

    ~TcpStream() {
        if (tcp_handle_) {
            uv_close(reinterpret_cast<uv_handle_t *>(tcp_handle_.get()),
                     [](uv_handle_t *handle) {
                         (void) handle;
                     });
        }
    }

private:
    std::unique_ptr<uv_tcp_t> tcp_handle_;

public:
    auto read(std::span<char> buf) const {
        struct ReadAwaiter {
            std::coroutine_handle<> handle_;
            uv_tcp_t               *socket_;
            std::span<char>         buf_;
            ssize_t                 nread_{0};

            ReadAwaiter(uv_tcp_t *socket, std::span<char> buf)
                : socket_{socket}
                , buf_{buf} {
                socket_->data = this;

                auto res = uv_read_start(
                    reinterpret_cast<uv_stream_t *>(socket_),
                    [](uv_handle_t *handle,
                       size_t       suggested_size,
                       uv_buf_t    *buf) {
                        (void) suggested_size;
                        auto data = static_cast<ReadAwaiter *>(handle->data);
                        *buf
                            = uv_buf_init(data->buf_.data(), data->buf_.size());
                    },
                    [](uv_stream_t *req, ssize_t nread, const uv_buf_t *buf) {
                        (void) buf;
                        auto data = static_cast<ReadAwaiter *>(req->data);
                        data->nread_ = nread;
                        if (nread > 0) {
                        } else if (nread < 0) {
                            if (nread != UV_EOF) {
                                console.error(
                                    "Read error: {}",
                                    uv_err_name(static_cast<int>(nread)));
                            } else {
                                console.warn("Tcp stream disconnected");
                            }
                        }

                        if (data->handle_) {
                            data->handle_.resume();
                        }
                    });

                assert(res == 0);
            }

            auto await_ready() const -> bool {
                return this->nread_ > 0;
            }
            auto await_suspend(std::coroutine_handle<> handle) {
                this->handle_ = handle;
            }
            auto await_resume() -> ssize_t {
                auto nread = this->nread_;
                this->nread_ = 0;
                this->handle_ = nullptr;
                socket_->data = nullptr;

                uv_read_stop(reinterpret_cast<uv_stream_t *>(socket_));
                return nread;
            }
        };

        return ReadAwaiter{tcp_handle_.get(), buf};
    }

    auto write(std::string_view message) {
        struct WriteAwaiter {
            std::coroutine_handle<> handle_;
            uv_tcp_t               *socket_;
            int                     status_{0};
            std::string             to_write_;
            uv_write_t              req{};

            WriteAwaiter(uv_tcp_t *socket, std::string_view message)
                : socket_{socket}
                , status_{1}
                , to_write_{message.data(), message.size()} {
                req.data = this;

                uv_buf_t buf = uv_buf_init(to_write_.data(), to_write_.size());

                int res = uv_write(&req,
                                   reinterpret_cast<uv_stream_t *>(socket_),
                                   &buf,
                                   1,
                                   [](uv_write_t *req, int status) {
                                       auto data = static_cast<WriteAwaiter *>(
                                           req->data);
                                       data->status_ = status;
                                       if (status != 0) {
                                           console.error("Write error: {}",
                                                         uv_strerror(status));
                                       }
                                       if (data->handle_) {
                                           data->handle_.resume();
                                       }
                                   });
                assert(res == 0);
            }

            auto await_ready() const noexcept -> bool {
                return status_ <= 0;
            }

            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                handle_ = handle;
            }

            auto await_resume() noexcept -> bool {
                handle_ = nullptr;
                return status_ == 0;
            }
        };

        return WriteAwaiter{tcp_handle_.get(), message};
    }
};

} // namespace uvio::net
