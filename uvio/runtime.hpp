#pragma once

#include "uvio/coroutine/task.hpp"
#include "uvio/log.hpp"
#include "uvio/work.hpp"

#include <functional>

#include "uv.h"

namespace uvio {

using namespace uvio::log;
using namespace uvio::work;

namespace detail {
    static inline auto run_loop() -> int {
        int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        uv_loop_close(uv_default_loop());
        return ret;
    }
} // namespace detail

static inline auto block_on(Task<> &&first_coro) {
    auto handle = std::move(first_coro).take();
    console.debug("loop run ...");
    handle.resume();

#if !defined(NDEBUG)
    console.debug("{:*^30}", "[all handles]");
    uv_print_all_handles(uv_default_loop(), stdout);
    console.debug("{:*^30}", "[active handles]");
    uv_print_active_handles(uv_default_loop(), stdout);
#endif
    uvio::detail::run_loop();
    console.debug("loop end.");
}

static inline auto spawn(Task<> &&task) {
    auto handle = std::move(task).take();
    console.debug("spawn task ...");
    handle.resume();
    console.debug("spawn end.");
}

static inline auto spawn(std::function<void()> &&func) {
    auto task = [func = std::move(func)]() -> Task<> {
        co_await execute(func);
    }();
    auto handle = task.take();
    handle.resume();
}

} // namespace uvio
