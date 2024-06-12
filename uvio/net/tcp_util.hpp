#pragma once

#include "uv.h"
#include <string>

namespace uvio::net {

class PeerAddress {
public:
    PeerAddress(uv_tcp_t *tcp_handle) {
        int namelen = sizeof(struct sockaddr);
        uv_tcp_getpeername(tcp_handle,
                           reinterpret_cast<struct sockaddr *>(&addrin_),
                           &namelen);
    }

public:
    auto ipv4() const -> std::string {
        std::string ip(17, 0);
        uv_ip4_name(&addrin_, ip.data(), ip.size());
        return ip;
    }

    inline auto port() const -> int {
        return addrin_.sin_port;
    }

private:
    struct sockaddr_in addrin_ {};
};
} // namespace uvio::net
