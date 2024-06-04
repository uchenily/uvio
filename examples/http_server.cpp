#include "uvio/debug.hpp"
#include "uvio/net/http.hpp"
#include "uvio/net/http/http_protocol.hpp"

using namespace uvio::net;
auto main() -> int {
    http::HttpServer server{"0.0.0.0", 8000};
    server.set_handler("/",
                       [](http::HttpRequest &req, http::HttpResponse &resp) {
                           resp.body = "hello";
                       });
    server.run();
}
