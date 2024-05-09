// origin: https://github.com/8sileus/zedio/blob/main/zedio/sync/latch.hpp
// https://github.com/LEAVING-7/Coco/blob/main/include/coco/sync/latch.hpp
#pragma once

#include "uvio/macros.hpp"
#include "uvio/runtime.hpp"

#include <atomic>

namespace uvio::sync {

class Latch {
    struct LatchAwaiter {
        std::coroutine_handle<> handle_;
        Latch                  *latch_;
        LatchAwaiter           *next_;

        LatchAwaiter(Latch *latch)
            : latch_{latch} {}

        auto await_ready() const noexcept -> bool {
            return latch_->try_wait();
        }

        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool {
            handle_ = handle;
            next_ = latch_->head_.load(std::memory_order::relaxed);
            while (!latch_->head_.compare_exchange_weak(
                next_,
                this,
                std::memory_order::acq_rel,
                std::memory_order::relaxed)) {
            }
            // return !latch_->try_wait();
            return true;
        }

        auto await_resume() const noexcept {}
    };

public:
    explicit Latch(std::ptrdiff_t expected)
        : expected_{expected} {}

    // Delete copy
    Latch(const Latch &) = delete;
    auto operator=(const Latch &) -> Latch & = delete;
    // Delete move
    Latch(Latch &&) = delete;
    auto operator=(Latch &&) -> Latch & = delete;

    auto count_down(std::ptrdiff_t update = 1) -> void {
        auto odd = expected_.fetch_sub(update, std::memory_order::acquire);
        if (odd == update) {
            notify_all();
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto wait() {
        return LatchAwaiter{this};
    }

    [[nodiscard]]
    auto try_wait() -> bool {
        return expected_.load(std::memory_order::acquire) == 0;
    }

    [[REMEMBER_CO_AWAIT]]
    auto arrive_and_wait(std::ptrdiff_t update = 1) {
        count_down(update);
        return wait();
    }

private:
    auto notify_all() -> void {
        auto head = head_.load(std::memory_order::acquire);
        while (head != nullptr) {
            head->handle_.resume();
            head = head->next_;
        }
    }

private:
    std::atomic<std::ptrdiff_t> expected_;
    std::atomic<LatchAwaiter *> head_{nullptr};
};

} // namespace uvio::sync
