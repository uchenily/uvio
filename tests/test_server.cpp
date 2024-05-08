#include "uvio/debug.hpp"
#include "uvio/log.hpp"

#include "uv.h"

#define HOST "0.0.0.0"
#define PORT 8000

using namespace uvio::log;

auto main() -> int {
    uv_tcp_t    server;
    sockaddr_in addr{};
    uv_tcp_init(uv_default_loop(), &server);
    uv_ip4_addr(HOST, PORT, &addr);
    uv_tcp_bind(&server, reinterpret_cast<const sockaddr *>(&addr), 0);

    // UV_EXTERN int uv_listen(uv_stream_t* stream, int backlog,
    // uv_connection_cb cb);
    uv_check(uv_listen(
        reinterpret_cast<uv_stream_t *>(&server),
        64,
        [](uv_stream_t *server, int status) {
            if (status < 0) {
                console.error("New connection error: {}", uv_strerror(status));
                return;
            }

            auto client = new uv_tcp_t;
            uv_tcp_init(uv_default_loop(), client);

            uv_check(
                uv_accept(server, reinterpret_cast<uv_stream_t *>(client)));

            // UV_EXTERN int uv_read_start(uv_stream_t*,
            //                             uv_alloc_cb alloc_cb,
            //                             uv_read_cb read_cb);
            uv_check(uv_read_start(
                reinterpret_cast<uv_stream_t *>(client),
                [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
                    (void) handle;
                    buf->base = new char[suggested_size];
                    buf->len = suggested_size;
                },
                [](uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
                    [](uv_stream_t    *client,
                       ssize_t         nread,
                       const uv_buf_t *buf) {
                        if (nread > 0) {
                            console.info("Received: {}",
                                         std::string(buf->base, nread));

                            auto req = new uv_write_t;

                            uv_buf_t uv_buf = uv_buf_init(buf->base, nread);

                            // UV_EXTERN int uv_write(uv_write_t* req,
                            //                        uv_stream_t* handle,
                            //                        const uv_buf_t bufs[],
                            //                        unsigned int nbufs,
                            //                        uv_write_cb cb);
                            uv_write(req,
                                     client,
                                     &uv_buf,
                                     1,
                                     [](uv_write_t *req, int status) {
                                         if (status != 0) {
                                             console.error("Write error: {}",
                                                           uv_strerror(status));
                                         }

                                         delete req;
                                     });

                            return;
                        } else if (nread < 0) {
                            if (nread != UV_EOF) {
                                console.error(
                                    "Read error: {}",
                                    uv_err_name(static_cast<int>(nread)));
                            } else {
                                console.warn("Client disconnected");
                            }
                            // UV_EXTERN void uv_close(uv_handle_t* handle,
                            // uv_close_cb close_cb);
                            uv_close(reinterpret_cast<uv_handle_t *>(client),
                                     [](uv_handle_t *handle) {
                                         delete reinterpret_cast<uv_tcp_t *>(
                                             handle);
                                     });
                        }

                        delete[] buf->base;
                    }(client, nread, buf);
                }));
        }));

    console.info("Listening on {}:{} ...", HOST, PORT);
    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
