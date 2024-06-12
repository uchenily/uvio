#include "uvio/debug.hpp"

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

// uv.h 不要放到第一行
// https://github.com/uchenily/uvio/actions/runs/9477525962/job/26112232763
#include "uv.h"

void on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void) handle;
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        if (nread != UV_EOF) {
            LOG_ERROR("Read error: {}", uv_strerror(static_cast<int>(nread)));
        }
        delete[] buf->base;
        uv_tty_reset_mode();
        uv_read_stop(stream);
        uv_close(reinterpret_cast<uv_handle_t *>(stream),
                 [](uv_handle_t *handle) {
                     (void) handle;
                 });
        return;
    }

    if (nread > 0) {
        // man ascii
        // 0x03 -> ETX (end of text)
        // 0x04 -> EOT (end of transmission)
        if (buf->base[0] == 0x03 || buf->base[0] == 0x04) {
            delete[] buf->base;
            uv_tty_reset_mode();
            uv_read_stop(stream);
            uv_close(reinterpret_cast<uv_handle_t *>(stream),
                     [](uv_handle_t *handle) {
                         (void) handle;
                     });
            LOG_INFO("exit!");
            return;
        }

        LOG_INFO("[READ]: {}", std::string_view{buf->base, (size_t) nread});
    }

    // delete[] buf->base;
    // uv_close(reinterpret_cast<uv_handle_t *>(stream), [](uv_handle_t *handle)
    // {
    //     (void) handle;
    // });
}

auto main() -> int {
    uv_tty_t tty_handle; // or new uv_tty_t
    uv_tty_init(uv_default_loop(), &tty_handle, STDIN_FILENO, 1);
    uv_tty_set_mode(&tty_handle, UV_TTY_MODE_RAW); // without buffer
    // uv_tty_set_mode(tty_handle, UV_TTY_MODE_NORMAL); // with buffer

    LOG_INFO("Press `Ctrl-D` to exit");
    uv_read_start(reinterpret_cast<uv_stream_t *>(&tty_handle),
                  on_alloc,
                  on_read);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
}
