// https://github.com/libuv/libuv/blob/v1.x/docs/code/ref-timer/main.c
#include "uvio/log.hpp"

#include "uv.h"

using namespace uvio::log;

auto main() -> int {
    uv_loop_t *loop = uv_default_loop();
    uv_timer_t gc_req;
    uv_timer_t fake_job_req;

    uv_timer_init(loop, &gc_req);
    uv_unref(reinterpret_cast<uv_handle_t *>(&gc_req));

    uv_timer_start(
        &gc_req,
        [](uv_timer_t *) {
            console.info("Freeing unused objects");
        },
        0,
        2000);

    // could actually be a TCP download or something
    uv_timer_init(loop, &fake_job_req);
    uv_timer_start(
        &fake_job_req,
        [](uv_timer_t *) {
            console.info("Fake job done");
        },
        9000,
        0);
    return uv_run(loop, UV_RUN_DEFAULT);
}
