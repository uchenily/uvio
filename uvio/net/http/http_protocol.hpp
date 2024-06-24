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
    // TODO(x): struct HttpStatus { status_code, status_text };
    int              status_code;
    std::string      status_text;
    http::HttpHeader headers;
    std::string      body;
};

} // namespace uvio::net::http
