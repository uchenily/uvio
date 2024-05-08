#pragma once

#include "uvio/net/tcp_stream.hpp"

#include <coroutine>
#include <memory>
#include <string_view>

#include <cassert>

#include <netinet/in.h>

#include "uv.h"

namespace uvio::net {

class TcpListener {

    struct AcceptAwaiter {

        std::coroutine_handle<>   handle_;
        uv_tcp_t                 *server_;
        std::unique_ptr<uv_tcp_t> client_;
        bool                      ready_{false};
        int                       status_{0};

        AcceptAwaiter(uv_tcp_t *server)
            : server_{server}
            , client_{std::make_unique<uv_tcp_t>()} {
            uv_tcp_init(uv_default_loop(), client_.get());

            server_->data = this;
        }

        auto await_ready() const -> bool {
            return ready_;
        }
        auto await_suspend(std::coroutine_handle<> handle) {
            handle_ = handle;
        }
        [[nodiscard]]
        auto await_resume() {
            auto res
                = uv_accept(reinterpret_cast<uv_stream_t *>(server_),
                            reinterpret_cast<uv_stream_t *>(client_.get()));
            assert(res == 0);

            return TcpStream{std::move(client_)};
        }
    };

    std::coroutine_handle<> handle_;
    uv_tcp_t                listen_socket_{};
    int                     backlog_{64};

public:
    TcpListener() {
        uv_tcp_init(uv_default_loop(), &listen_socket_);
    }

    ~TcpListener() {
        uv_close(reinterpret_cast<uv_handle_t *>(&listen_socket_),
                 [](uv_handle_t *handle) {
                     (void) handle;
                 });
    }

public:
    auto bind(std::string_view addr, int port) {
        struct sockaddr_in bind_addr {};
        uv_ip4_addr(addr.data(), port, &bind_addr);
        uv_tcp_bind(&listen_socket_,
                    reinterpret_cast<const sockaddr *>(&bind_addr),
                    0);

        uv_listen(reinterpret_cast<uv_stream_t *>(&listen_socket_),
                  backlog_,
                  [](uv_stream_t *req, int status) {
                      auto data = static_cast<AcceptAwaiter *>(req->data);
                      data->status_ = status;
                      assert(status == 0);
                      data->ready_ = true;
                      if (data->handle_) {
                          data->handle_.resume();
                      }
                  });
    }

    auto accept() noexcept {
        return AcceptAwaiter{&listen_socket_};
    }
};

} // namespace uvio::net
