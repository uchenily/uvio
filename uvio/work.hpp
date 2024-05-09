#pragma once

#include "uvio/debug.hpp"
#include "uvio/macros.hpp"

#include <coroutine>
#include <functional>

#include "uv.h"

namespace uvio::work {

namespace detail {

    struct WorkAwaiter {
        std::coroutine_handle<> handle_;
        uv_work_t               work_{};
        std::function<void()>   func_;
        int                     status_{1};

        WorkAwaiter(std::function<void()> &&func)
            : func_{std::move(func)} {
            work_.data = this;

            uv_check(uv_queue_work(
                uv_default_loop(),
                &work_,
                [](uv_work_t *work_req) {
                    auto data = static_cast<WorkAwaiter *>(work_req->data);
                    data->func_();
                },
                [](uv_work_t *work_req, int status) {
                    auto data = static_cast<WorkAwaiter *>(work_req->data);
                    data->status_ = status;

                    if (data->handle_) {
                        data->handle_.resume();
                    }
                }));
        }

        auto await_ready() const noexcept -> bool {
            return status_ <= 0;
        }

        auto await_suspend(std::coroutine_handle<> handle) noexcept {
            handle_ = handle;
        }

        auto await_resume() noexcept {
            handle_ = nullptr;
        };
    };

} // namespace detail

[[REMEMBER_CO_AWAIT]]
static inline auto execute(std::function<void()> func) {
    return detail::WorkAwaiter{std::move(func)};
}

} // namespace uvio::work
