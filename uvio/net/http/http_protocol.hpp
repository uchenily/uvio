#pragma once

#include "uvio/net/http/http_util.hpp"

#include <regex>
#include <string>

namespace uvio::net::http {

struct HttpRequest {
    std::string      method;
    std::string      uri;
    http::HttpHeader headers{};
    std::string      body{};

    std::smatch match_{};

    auto get_param(std::size_t index) const -> std::string {
        // match_[0] represent the entire string
        if (index + 1 < match_.size()) {
            return match_[index + 1];
        }
        return "";
    }
};

struct HttpResponse {
    // TODO(x): struct HttpStatus { status_code, status_text };
    int              status_code;
    std::string      status_text{};
    http::HttpHeader headers{};
    std::string      body{};
};

} // namespace uvio::net::http
