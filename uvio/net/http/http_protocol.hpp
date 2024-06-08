#pragma once

#include "uvio/net/http/http_util.hpp"
#include <string>

namespace uvio::net::http {

struct HttpRequest {
    std::string method;
    std::string uri;
    HttpHeader  headers;
    std::string body;
};

struct HttpResponse {
    int              status_code;
    http::HttpHeader headers;
    std::string      body;
};

} // namespace uvio::net::http
