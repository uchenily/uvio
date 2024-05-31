#pragma once

#include "uvio/common/result.hpp"
#include "uvio/debug.hpp"
#include "uvio/macros.hpp"
#include "uvio/net/http/http_protocol.hpp"

#include <coroutine>

#include "curl/curl.h"
#include "curl/multi.h"
#include "uv.h"

namespace uvio::net::http {

class HttpClient {
    struct Context {
        const std::coroutine_handle<> &handle_;
        uv_timer_t                     timer_;
        uv_poll_t                      poll_handle;
        curl_socket_t                  curl_sockfd;
        CURLM                         *multi_handle_;
        std::string                    response_body_;
        int                            response_code_{0};

        Context(std::coroutine_handle<> &handle)
            : handle_{handle} {}
    };

public:
    [[REMEMBER_CO_AWAIT]]
    static auto request(const HttpRequest &req) {
        struct RequestAwaiter {
            RequestAwaiter(const HttpRequest &req) noexcept {
                uv_timer_init(uv_default_loop(), &context_.timer_);
                context_.timer_.data = this;
                context_.multi_handle_ = curl_multi_init();

                curl_multi_setopt(context_.multi_handle_,
                                  CURLMOPT_SOCKETFUNCTION,
                                  curl_socket_cb);
                curl_multi_setopt(context_.multi_handle_,
                                  CURLMOPT_SOCKETDATA,
                                  this);

                curl_multi_setopt(context_.multi_handle_,
                                  CURLMOPT_TIMERFUNCTION,
                                  curl_timer_cb);
                curl_multi_setopt(context_.multi_handle_,
                                  CURLMOPT_TIMERDATA,
                                  this);

                http_request(req.url);
            }

            void http_request(std::string_view url) {
                CURL *curl_handle = curl_easy_init();
                curl_easy_setopt(curl_handle, CURLOPT_URL, url.data());
                // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
                curl_easy_setopt(curl_handle,
                                 CURLOPT_WRITEFUNCTION,
                                 curl_write_cb);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);
                curl_multi_add_handle(context_.multi_handle_, curl_handle);
            }

            static void check_multi_info(Context *context) {
                char    *done_url = nullptr;
                CURLMsg *message = nullptr;
                int      pending = 0;
                int      response_code = 0;

                while ((message = curl_multi_info_read(context->multi_handle_,
                                                       &pending))) {
                    switch (message->msg) {
                    case CURLMSG_DONE:
                        curl_easy_getinfo(message->easy_handle,
                                          CURLINFO_EFFECTIVE_URL,
                                          &done_url);
                        LOG_DEBUG("curl {} done!", done_url);

                        curl_easy_getinfo(message->easy_handle,
                                          CURLINFO_RESPONSE_CODE,
                                          &response_code);
                        LOG_DEBUG("response code: {}", response_code);
                        context->response_code_ = response_code;

                        curl_multi_remove_handle(context->multi_handle_,
                                                 message->easy_handle);
                        curl_easy_cleanup(message->easy_handle);

                        if (context->handle_) {
                            context->handle_.resume();
                        }
                        break;

                    default:
                        abort();
                    }
                }
            }

            static void curl_poll_cb(uv_poll_t *req, int status, int events) {
                // req->data -> Context * 不是 RequestAwaiter *
                auto context = static_cast<Context *>(req->data);
                uv_timer_stop(&context->timer_);
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

                curl_multi_socket_action(context->multi_handle_,
                                         context->curl_sockfd,
                                         flags,
                                         &running_handles);
                check_multi_info(context);
            }

            static void timer_cb(uv_timer_t *req) {
                auto data = static_cast<RequestAwaiter *>(req->data);
                int  running_handles = 0;
                curl_multi_socket_action(data->context_.multi_handle_,
                                         CURL_SOCKET_TIMEOUT,
                                         0,
                                         &running_handles);
                check_multi_info(&data->context_);
            }

            static void
            curl_timer_cb(CURLM * /*multi*/, int64_t timeout_ms, void *userp) {
                auto data = static_cast<RequestAwaiter *>(userp);
                if (timeout_ms <= 0) {
                    timeout_ms = 1;
                }
                uv_timer_start(&data->context_.timer_, timer_cb, timeout_ms, 0);
            }

            static auto curl_write_cb(char  *data,
                                      size_t size,
                                      size_t nmemb,
                                      void  *user_data) -> size_t {
                assert(user_data != nullptr);
                auto client = static_cast<RequestAwaiter *>(user_data);
                client->context_.response_body_.append(data, size * nmemb);
                return size * nmemb;
            }

            static auto curl_socket_cb(CURL * /*easy*/,
                                       curl_socket_t curl_sockfd,
                                       int           action,
                                       void         *client,
                                       void         *context) -> int {
                auto data = static_cast<RequestAwaiter *>(client);

                if (action == CURL_POLL_IN || action == CURL_POLL_OUT) {
                    if (!context) {
                        data->context_.poll_handle.data = &data->context_;
                        data->context_.curl_sockfd = curl_sockfd;

                        uv_check(
                            uv_poll_init_socket(uv_default_loop(),
                                                &data->context_.poll_handle,
                                                curl_sockfd));
                        curl_multi_assign(data->context_.multi_handle_,
                                          curl_sockfd,
                                          &data->context_);
                    }
                }

                switch (action) {
                case CURL_POLL_IN:
                    uv_poll_start(&data->context_.poll_handle,
                                  UV_READABLE,
                                  curl_poll_cb);
                    break;
                case CURL_POLL_OUT:
                    uv_poll_start(&data->context_.poll_handle,
                                  UV_WRITABLE,
                                  curl_poll_cb);
                    break;
                case CURL_POLL_REMOVE:
                    if (context) {
                        uv_poll_stop(&data->context_.poll_handle);
                        curl_multi_assign(data->context_.multi_handle_,
                                          curl_sockfd,
                                          nullptr);
                    }
                    break;
                default:
                    abort();
                }

                return 0;
            }

        public:
            auto await_ready() const noexcept -> bool {
                return context_.response_code_ != 0;
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                handle_ = handle;
            }
            [[nodiscard]]
            auto await_resume() const noexcept -> Result<HttpResponse> {
                return HttpResponse{.http_code = context_.response_code_,
                                    .body = context_.response_body_};
            }

        private:
            std::coroutine_handle<> handle_;
            Context                 context_{handle_};
        };

        if (!curl_inited_) {
            if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
                LOG_ERROR("Cloud not init cURL");
            }
            curl_inited_ = true;
        }
        return RequestAwaiter{req};
    }

private:
    static inline bool curl_inited_{false};
};
} // namespace uvio::net::http
