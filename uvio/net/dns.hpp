#pragma once

#include "uvio/common/result.hpp"
#include "uvio/debug.hpp"
#include "uvio/macros.hpp"

#include <coroutine>
#include <string_view>

#include "uv.h"

namespace uvio::net {

class DNS {

    struct ResolveAwaiter {

        std::coroutine_handle<> handle_;
        uv_getaddrinfo_t        req_{};
        std::string             address_;
        std::string             service_;
        struct addrinfo         hints_ {};
        std::array<char, 17>    ipv4addr_{};
        bool                    resolved_{false};

        ResolveAwaiter(std::string_view address, std::string_view service)
            : address_{address}
            , service_{service} {
            hints_.ai_family = PF_INET;
            hints_.ai_socktype = SOCK_STREAM;
            hints_.ai_protocol = IPPROTO_TCP;
            hints_.ai_flags = 0;

            req_.data = this;

            uv_getaddrinfo(
                uv_default_loop(),
                &req_,
                [](uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
                    auto data = static_cast<ResolveAwaiter *>(req->data);
                    if (status < 0) {
                        LOG_ERROR("uv_getaddrinfo callback error {}",
                                  uv_err_name(status));
                    } else {
                        uv_ip4_name(reinterpret_cast<struct sockaddr_in *>(
                                        res->ai_addr),
                                    data->ipv4addr_.data(),
                                    16);
                        uv_freeaddrinfo(res);
                    }
                    data->resolved_ = true;
                    if (data->handle_) {
                        data->handle_.resume();
                    }
                },
                address_.data(),
                service_.data(),
                &hints_);
        }

        auto await_ready() const -> bool {
            return resolved_;
        }
        auto await_suspend(std::coroutine_handle<> handle) {
            handle_ = handle;
        }
        [[nodiscard]]
        auto await_resume() const -> Result<std::string> {
            auto ip_address
                = std::string{ipv4addr_.data(), std::strlen(ipv4addr_.data())};
            if (!ip_address.empty()) {
                return ip_address;
            }
            return unexpected{make_uvio_error(Error::Unclassified)};
        }
    };

public:
    [[REMEMBER_CO_AWAIT]]
    static auto resolve(std::string_view address,
                        std::string_view service) noexcept {
        return ResolveAwaiter{address, service};
    }
};

} // namespace uvio::net
