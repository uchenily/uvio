#pragma once

#include <coroutine>
#include <exception>

namespace uvio {

template <typename T>
class Task;

namespace detail {
    template <typename T>
    class TaskPromise {
    public:
        auto get_return_object() noexcept -> Task<T>;

        auto initial_suspend() const noexcept -> std::suspend_always {
            return {};
        }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                auto await_ready() const noexcept -> bool {
                    return false;
                }
                auto await_suspend(std::coroutine_handle<TaskPromise> callee)
                    const noexcept -> std::coroutine_handle<> {
                    if (callee.promise().caller_ != nullptr) {
                        return callee.promise().caller_;
                    } else {
                        callee.destroy();
                        return std::noop_coroutine();
                    }
                }
                auto await_resume() const noexcept {}
            };
            return FinalAwaiter{};
        }

        auto return_void() const noexcept {}

        auto unhandled_exception() {
            std::terminate();
        }

    public:
        std::coroutine_handle<> caller_{nullptr};
    };

} // namespace detail

template <class T = void>
class Task {
public:
    using promise_type = uvio::detail::TaskPromise<T>;

    Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

public:
    auto operator co_await() const & noexcept {
        struct Awaitable {
            std::coroutine_handle<promise_type> callee_;

            Awaitable(std::coroutine_handle<promise_type> callee) noexcept
                : callee_{callee} {}

            auto await_ready() const noexcept -> bool {
                return !callee_ || callee_.done();
            }

            auto await_suspend(std::coroutine_handle<> caller)
                -> std::coroutine_handle<> {
                callee_.promise().caller_ = caller;
                return callee_;
            }

            auto await_resume() const noexcept {}
        };
        return Awaitable{handle_};
    }

    auto take() -> std::coroutine_handle<promise_type> {
        if (handle_ == nullptr) [[unlikely]] {
            std::terminate();
        }
        auto res = std::move(handle_);
        handle_ = nullptr;
        return res;
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

template <typename T>
inline auto uvio::detail::TaskPromise<T>::get_return_object() noexcept
    -> Task<T> {
    return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}

} // namespace uvio
