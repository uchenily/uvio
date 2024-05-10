#include "uvio/log.hpp"

// #include <netdb.h>

#include "uv.h"

using namespace uvio::log;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void) handle;
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        if (nread != UV_EOF) {
            console.error("Read error {}",
                          uv_err_name(static_cast<int>(nread)));
        }
        uv_close(reinterpret_cast<uv_handle_t *>(client), nullptr);
        delete buf->base;
        delete reinterpret_cast<uv_tcp_t *>(client);
        return;
    }

    console.info(std::string_view{buf->base, static_cast<size_t>(nread)});
    // delete buf->base;
    // delete reinterpret_cast<uv_tcp_t *>(client);
    uv_close(reinterpret_cast<uv_handle_t *>(client), [](uv_handle_t *handle) {
        delete reinterpret_cast<uv_tcp_t *>(handle);
    });
}

void on_connect(uv_connect_t *req, int status) {
    if (status < 0) {
        console.error("connect failed error {}", uv_err_name(status));
        delete req;
        return;
    }

    uv_read_start(reinterpret_cast<uv_stream_t *>(req->handle),
                  alloc_buffer,
                  on_read);
    delete req;
}

void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
    (void) resolver;
    if (status < 0) {
        console.error("uv_getaddrinfo callback error {}", uv_err_name(status));
        return;
    }

    std::array<char, 17> addr{};
    uv_ip4_name(reinterpret_cast<struct sockaddr_in *>(res->ai_addr),
                addr.data(),
                16);
    console.info(addr.data());

    auto connect_req = new uv_connect_t;
    auto socket = new uv_tcp_t;
    uv_tcp_init(uv_default_loop(), socket);

    uv_tcp_connect(connect_req,
                   socket,
                   reinterpret_cast<const struct sockaddr *>(res->ai_addr),
                   on_connect);

    uv_freeaddrinfo(res);
}

auto main() -> int {
    struct addrinfo hints {};
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    uv_getaddrinfo_t resolver;
    console.info("irc.libera.chat is... ");
    int ret = uv_getaddrinfo(uv_default_loop(),
                             &resolver,
                             on_resolved,
                             "irc.libera.chat",
                             "6667",
                             &hints);

    if (ret != 0) {
        console.error("uv_getaddrinfo call error %s\n", uv_err_name(ret));
        return 1;
    }
    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
