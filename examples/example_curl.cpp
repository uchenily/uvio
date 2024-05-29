#include "uvio/debug.hpp"

#include <string_view>

#include "curl/curl.h"
#include "uv.h"
#include <curl/easy.h>
#include <curl/multi.h>

uv_timer_t timer;
CURLM     *multi_handle;

using curl_context_t = struct curl_context_s {
    uv_poll_t     poll_handle;
    curl_socket_t curl_sockfd;
};

auto create_curl_context(curl_socket_t sockfd) -> curl_context_t * {
    auto curl_context = new curl_context_t{};

    curl_context->curl_sockfd = sockfd;

    uv_check(uv_poll_init_socket(uv_default_loop(),
                                 &curl_context->poll_handle,
                                 sockfd));
    curl_context->poll_handle.data = curl_context;

    return curl_context;
}

void curl_close_cb(uv_handle_t *handle) {
    auto curl_context = static_cast<curl_context_t *>(handle->data);
    delete curl_context;
}

void destroy_curl_context(curl_context_t *curl_context) {
    uv_close(reinterpret_cast<uv_handle_t *>(&curl_context->poll_handle),
             curl_close_cb);
}

void http_request(std::string_view url) {
    CURL *curl_handle = curl_easy_init();
    // curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.data());
    curl_multi_add_handle(multi_handle, curl_handle);
}

void check_multi_info() {
    char    *done_url = nullptr;
    CURLMsg *message = nullptr;
    int      pending = 0;

    while ((message = curl_multi_info_read(multi_handle, &pending))) {
        switch (message->msg) {
        case CURLMSG_DONE:
            curl_easy_getinfo(message->easy_handle,
                              CURLINFO_EFFECTIVE_URL,
                              &done_url);
            LOG_DEBUG("curl {} done!", done_url);

            curl_multi_remove_handle(multi_handle, message->easy_handle);
            curl_easy_cleanup(message->easy_handle);
            break;

        default:
            LOG_ERROR("curl msg {}", (int) message->msg);
            abort();
        }
    }
}

void curl_poll_cb(uv_poll_t *req, int status, int events) {
    uv_timer_stop(&timer);
    int running_handles = 0;
    int flags = 0;
    if (status < 0) {
        flags = CURL_CSELECT_ERR;
    }
    if ((status == 0) && ((events & UV_READABLE) != 0)) {
        flags |= CURL_CSELECT_IN;
    }
    if ((status == 0) && ((events & UV_WRITABLE) != 0)) {
        flags |= CURL_CSELECT_OUT;
    }

    auto context = static_cast<curl_context_t *>(req->data);

    curl_multi_socket_action(multi_handle,
                             context->curl_sockfd,
                             flags,
                             &running_handles);
    check_multi_info();
}

void timer_cb(uv_timer_t * /*req*/) {
    int running_handles = 0;
    curl_multi_socket_action(multi_handle,
                             CURL_SOCKET_TIMEOUT,
                             0,
                             &running_handles);
    check_multi_info();
}

void curl_timer_cb(CURLM * /*multi*/, int64_t timeout_ms, void * /*userp*/) {
    LOG_DEBUG("curl_timer_cb()");
    if (timeout_ms <= 0) {
        timeout_ms = 1;
    }
    uv_timer_start(&timer, timer_cb, timeout_ms, 0);
}

// curl socketfd (readable/writable) -> curl_socket_cb() -> uv_poll_start() ->
// curl_poll_cb() -> curl_multi_socket_action()
auto curl_socket_cb(CURL * /*easy*/,
                    curl_socket_t curl_sockfd,
                    int           action,
                    void * /*client*/,
                    void *context) -> int {
    curl_context_t *curl_context = nullptr;

    if (action == CURL_POLL_IN || action == CURL_POLL_OUT) {
        if (context) {
            curl_context = static_cast<curl_context_t *>(context);
        } else {
            curl_context = create_curl_context(curl_sockfd);
            curl_multi_assign(multi_handle, curl_sockfd, curl_context);
        }
    }

    switch (action) {
    case CURL_POLL_IN:
        uv_poll_start(&curl_context->poll_handle, UV_READABLE, curl_poll_cb);
        break;
    case CURL_POLL_OUT:
        uv_poll_start(&curl_context->poll_handle, UV_WRITABLE, curl_poll_cb);
        break;
    case CURL_POLL_REMOVE:
        if (context) {
            uv_poll_stop(
                &(static_cast<curl_context_t *>(context))->poll_handle);
            destroy_curl_context(static_cast<curl_context_t *>(context));
            curl_multi_assign(multi_handle, curl_sockfd, nullptr);
        }
        break;
    default:
        abort();
    }

    return 0;
}

auto main(int argc, char **argv) -> int {
    if (argc <= 1) {
        return 0;
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        LOG_ERROR("Could not init cURL");
        return 1;
    }

    uv_timer_init(uv_default_loop(), &timer);

    multi_handle = curl_multi_init();
    curl_multi_setopt(multi_handle, CURLMOPT_SOCKETFUNCTION, curl_socket_cb);
    curl_multi_setopt(multi_handle, CURLMOPT_TIMERFUNCTION, curl_timer_cb);

    http_request(argv[1]);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    curl_multi_cleanup(multi_handle);
}
