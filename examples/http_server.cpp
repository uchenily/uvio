#include "uvio/net/http/http_server.hpp"
#include "uvio/debug.hpp"

using namespace uvio::net;
auto main() -> int {
    http::HttpServer server{"127.0.0.1", 8000};
    server.run();
}
