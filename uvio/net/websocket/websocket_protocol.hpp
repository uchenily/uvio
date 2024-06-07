#pragma once

#include "uvio/net/http/http_util.hpp"
#include "uvio/net/websocket/websocket_util.hpp"
#include <string>

namespace uvio::net::websocket {

struct HttpRequest {
    std::string      method;
    std::string      uri;
    http::HttpHeader headers;
    std::string      body;
};

struct HttpResponse {
    int              status_code;
    http::HttpHeader headers;
    std::string      body;
};

} // namespace uvio::net::websocket
