#include "uvio/log.hpp"

#include "uv.h"

using namespace uvio::log;

void on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void) handle;
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        if (nread != UV_EOF) {
            console.error("读取错误: {}", uv_strerror(static_cast<int>(nread)));
        }
        delete[] buf->base;
        uv_close(reinterpret_cast<uv_handle_t *>(stream),
                 [](uv_handle_t *handle) {
                     (void) handle;
                 });
        return;
    }

    if (nread > 0) {
        std::string const response(buf->base, nread);
        console.info("接收到响应: {}", response);
    }

    delete[] buf->base;
    uv_close(reinterpret_cast<uv_handle_t *>(stream), [](uv_handle_t *handle) {
        (void) handle;
    });
}

void on_connect(uv_connect_t *req, int status) {
    if (status < 0) {
        console.error("连接错误: {}", uv_strerror(status));
        return;
    }
    console.info("TCP客户端已连接...");

    uv_stream_t   *stream = req->handle;
    std::string    serialized_request{"hello"};
    uv_buf_t const wrbuf
        = uv_buf_init(serialized_request.data(), serialized_request.size());
    auto *write_req = new uv_write_t;
    uv_write(write_req, stream, &wrbuf, 1, [](uv_write_t *req, int status) {
        if (status < 0) {
            console.error("写入错误: {}", uv_strerror(status));
            return;
        }
        delete req;
    });

    uv_read_start(stream, on_alloc, on_read);
}

auto main() -> int {
    uv_tcp_t socket;
    uv_tcp_init(uv_default_loop(), &socket);

    struct sockaddr_in dest {};
    uv_ip4_addr("127.0.0.1", 8000, &dest);

    uv_connect_t connect_req;
    uv_tcp_connect(&connect_req,
                   &socket,
                   reinterpret_cast<const struct sockaddr *>(&dest),
                   on_connect);

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
