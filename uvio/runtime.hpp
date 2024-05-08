#pragma once

#include "uvio/coroutine/task.hpp"
#include "uvio/log.hpp"

#include "uv.h"

namespace uvio {

using namespace uvio::log;

namespace detail {
    static inline auto run_loop() -> int {
        int ret = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        uv_loop_close(uv_default_loop());
        return ret;
    }
} // namespace detail

static inline auto block_on(Task<> &&first_coro) {
    auto handle = first_coro.take();
    console.debug("loop run ...");
    handle.resume();

    uvio::detail::run_loop();
    console.debug("loop end.");
}

static inline auto spawn(Task<> &&task) {
    auto handle = task.take();
    console.debug("spawn task ...");
    handle.resume();
    console.debug("spawn end.");
}

} // namespace uvio
