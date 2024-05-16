#pragma once

#include "uvio/debug.hpp"
#include "uvio/log.hpp"
#include "uvio/macros.hpp"

#include <coroutine>
#include <memory>

#include "uv.h"

namespace uvio::net {

using namespace uvio::log;

class TcpStream {
public:
    TcpStream(std::unique_ptr<uv_tcp_t> socket)
        : tcp_handle_{std::move(socket)} {}

    TcpStream(TcpStream &&other) noexcept {
        if (std::addressof(other) != this) {
            tcp_handle_ = std::move(other.tcp_handle_);
            other.tcp_handle_ = nullptr;
        }
    }
    auto operator=(TcpStream &&other) noexcept -> TcpStream & {
        this->tcp_handle_ = std::move(other.tcp_handle_);
        other.tcp_handle_ = nullptr;
        return *this;
    }

    TcpStream(const TcpStream &) = delete;
    auto operator=(const TcpStream &) = delete;

    ~TcpStream() {
        // uv__finish_close: Assertion `0' failed.
        // Aborted (core dumped)
        // if (tcp_handle_) {
        //     uv_close(reinterpret_cast<uv_handle_t *>(tcp_handle_.get()),
        //              [](uv_handle_t *handle) {
        //                  (void) handle;
        //              });
        // }

        if (tcp_handle_) {
            uv_close(reinterpret_cast<uv_handle_t *>(tcp_handle_.release()),
                     [](uv_handle_t *handle) {
                         delete reinterpret_cast<uv_tcp_t *>(handle);
                     });
        }
    }

private:
    std::unique_ptr<uv_tcp_t> tcp_handle_;

public:
    [[REMEMBER_CO_AWAIT]]
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

                uv_check(uv_read_start(
                    reinterpret_cast<uv_stream_t *>(socket_),
                    [](uv_handle_t *handle,
                       size_t       suggested_size,
                       uv_buf_t    *buf) {
                        (void) suggested_size;
                        auto data = static_cast<ReadAwaiter *>(handle->data);
                        LOG_DEBUG("ReadAwaiter data->buf_.size(): {}",
                                  data->buf_.size());
                        *buf
                            = uv_buf_init(data->buf_.data(), data->buf_.size());
                    },
                    [](uv_stream_t *req, ssize_t nread, const uv_buf_t *buf) {
                        (void) buf;
                        auto data = static_cast<ReadAwaiter *>(req->data);
                        data->nread_ = nread;
                        if (nread < 0) {
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
                    }));
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

    [[REMEMBER_CO_AWAIT]]
    auto write(std::string_view message) {
        struct WriteAwaiter {
            std::coroutine_handle<> handle_;
            uv_tcp_t               *socket_;
            int                     status_{1};
            std::string             to_write_;
            uv_write_t              req{};

            WriteAwaiter(uv_tcp_t *socket, std::string_view message)
                : socket_{socket}
                , to_write_{message.data(), message.size()} {
                req.data = this;

                uv_buf_t buf = uv_buf_init(to_write_.data(), to_write_.size());

                uv_check(uv_write(&req,
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
                                  }));
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

public:
    [[REMEMBER_CO_AWAIT]]
    static auto connect(std::string_view addr, int port) {
        struct ConnectAwaiter {

            std::coroutine_handle<>   handle_;
            std::unique_ptr<uv_tcp_t> client_;
            uv_connect_t              connect_req_{};
            int                       status_{1};

            ConnectAwaiter(std::string_view addr, int port)
                : client_{std::make_unique<uv_tcp_t>()} {
                uv_tcp_init(uv_default_loop(), client_.get());

                struct sockaddr_in dest {};
                uv_ip4_addr(addr.data(), port, &dest);

                connect_req_.data = this;

                uv_check(uv_tcp_connect(
                    &connect_req_,
                    client_.get(),
                    reinterpret_cast<const struct sockaddr *>(&dest),
                    [](uv_connect_t *req, int status) {
                        auto data = static_cast<ConnectAwaiter *>(req->data);
                        data->status_ = status;
                        if (status != 0) {
                            console.error("Connect failed: {}",
                                          uv_strerror(status));
                            return;
                        }

                        if (data->handle_) {
                            data->handle_.resume();
                        }
                    }));
            }

            auto await_ready() const noexcept -> bool {
                return status_ <= 0;
            }

            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                handle_ = handle;
            }

            [[nodiscard]]
            auto await_resume() noexcept -> TcpStream {
                handle_ = nullptr;
                return TcpStream{std::move(client_)};
            }
        };

        return ConnectAwaiter{addr, port};
    }
};

} // namespace uvio::net
