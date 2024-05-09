#pragma once

#include "uvio/macros.hpp"

#include <chrono>
#include <coroutine>

#include "uv.h"

namespace uvio::time {

using namespace std::chrono_literals;

namespace detail {

    struct SleepAwaiter {
        std::coroutine_handle<> handle_;
        uv_timer_t              timer_{};
        bool                    ready_{false};

        SleepAwaiter(uint64_t timeout) {
            uv_timer_init(uv_default_loop(), &timer_);
            // uv_handle_set_data(reinterpret_cast<uv_handle_t *>(&timer_),
            // this);
            timer_.data = this;
            uv_timer_start(
                &timer_,
                [](uv_timer_t *uv_timer) {
                    auto data = static_cast<SleepAwaiter *>(uv_timer->data);
                    data->ready_ = true;
                    if (data->handle_) {
                        data->handle_.resume();
                    }
                },
                timeout,
                0);
        }

        auto await_ready() const noexcept -> bool {
            return ready_;
        }

        auto await_suspend(std::coroutine_handle<> handle) noexcept {
            handle_ = handle;
        }

        auto await_resume() noexcept {
            handle_ = nullptr;
            ready_ = false;
        };
    };

} // namespace detail

[[REMEMBER_CO_AWAIT]]
static inline auto sleep(const std::chrono::milliseconds &duration) {
    return detail::SleepAwaiter{static_cast<uint64_t>(duration.count())};
}

} // namespace uvio::time
