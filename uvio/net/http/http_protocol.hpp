#pragma once

#include <string>

namespace uvio::net::http {

struct HttpRequest {
    std::string method{"GET"};
    std::string url;
    std::string body;
};

struct HttpResponse {
    int         http_code;
    std::string body;
};

} // namespace uvio::net::http
