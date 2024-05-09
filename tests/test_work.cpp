#include "uvio/log.hpp"

#include <array>
#include <random>
#include <thread>

#include "uv.h"

#define FIB_UNTIL 25

using namespace uvio::log;

auto fibonacci(int t) -> int {
    if (t < 2) {
        return 1;
    } else {
        return fibonacci(t - 1) + fibonacci(t - 2);
    }
}

void fib(uv_work_t *req) {
    std::random_device              rd;
    std::mt19937                    g(rd());
    std::uniform_int_distribution<> random(100, 1000);

    int interval = random(g);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));

    auto n = *static_cast<int *>(req->data);
    auto ret = fibonacci(n);
    console.info("fibonacci({}) = {}", n, ret);
}

void after_fib(uv_work_t *req, int status) {
    (void) status;
    console.info("Done calculating {}th fibonacci",
                 *static_cast<int *>(req->data));
}

auto main() -> int {
    std::array<int, FIB_UNTIL>       data{};
    std::array<uv_work_t, FIB_UNTIL> work_reqs{};
    for (int i = 0; i < FIB_UNTIL; i++) {
        data.at(i) = i;
        work_reqs.at(i).data = &data.at(i);
        uv_queue_work(uv_default_loop(), &work_reqs.at(i), fib, after_fib);
    }

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
