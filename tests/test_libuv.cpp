#include "uv.h"

auto main() -> int {
    uv_loop_t *loop = uv_default_loop();
    uv_loop_init(loop);

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    return 0;
}
