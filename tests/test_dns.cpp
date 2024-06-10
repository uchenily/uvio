#include "uvio/debug.hpp"

#include "uv.h"

using namespace uvio::log;

void on_resolved(uv_getaddrinfo_t * /*resolver*/,
                 int              status,
                 struct addrinfo *res) {
    if (status < 0) {
        LOG_ERROR("uv_getaddrinfo callback error {}", uv_err_name(status));
        return;
    }

    std::array<char, 17> ipv4addr{};
    uv_ip4_name(reinterpret_cast<struct sockaddr_in *>(res->ai_addr),
                ipv4addr.data(),
                16);
    LOG_INFO("{}", ipv4addr.data());
    uv_freeaddrinfo(res);
}

auto main() -> int {
    struct addrinfo hints {};
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    uv_getaddrinfo_t req;
    LOG_INFO("resolving address `baidu.com` ...");
    int ret = uv_getaddrinfo(uv_default_loop(),
                             &req,
                             on_resolved,
                             "baidu.com",
                             "443",
                             &hints);

    if (ret != 0) {
        LOG_ERROR("uv_getaddrinfo call error %s\n", uv_err_name(ret));
        return 1;
    }
    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
