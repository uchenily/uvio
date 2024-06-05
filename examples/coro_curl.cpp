#include "uvio/core.hpp"
#include "uvio/log.hpp"
#include "uvio/net.hpp"
#include "uvio/net/http.hpp"

using namespace uvio;
using namespace uvio::log;
using namespace uvio::net;
using namespace uvio::net::http;

// curl http://httpbin.org/json
auto client() -> Task<> {
    HttpRequest req = {
        .method = "GET",
        // TODO(x)
        .uri = "http://httpbin.org/json",
    };
    auto ret = co_await HttpClient::request(req);
    if (!ret) {
        console.error("{}", ret.error().message());
        co_return;
    }
    auto response = std::move(ret.value());
    console.info("response [http_code={}] [body_length={}]",
                 response.http_code,
                 response.body.size());
    console.info("`{}`", response.body);
}

auto main() -> int {
    block_on(client());
}
