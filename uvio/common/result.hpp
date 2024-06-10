#pragma once

#include "uvio/common/expected.hpp"

#include <string_view>

#include <cassert>
#include <cstring>

namespace uvio {

class Error {
public:
    enum ErrorCode {
        UnexpectedEOF = 8000,
        WriteZero,
        ResolvedFailed,
        Unclassified,
    };

public:
    explicit Error(int error_code)
        : error_code_{error_code} {}

public:
    [[nodiscard]]
    auto message() const noexcept -> std::string_view {
        switch (error_code_) {
        case UnexpectedEOF:
            return "Read EOF too early";
        case WriteZero:
            return "Write return zero";
        case ResolvedFailed:
            return "DNS resolve failed";
        case Unclassified:
            return "Unclassified error";
        default:
            return strerror(error_code_);
        }
    }

private:
    int error_code_;
};

[[nodiscard]]
static inline auto make_uvio_error(int error_code) -> Error {
    assert(error_code >= 8000);
    return Error{error_code};
}

[[nodiscard]]
static inline auto make_sys_error(int error_code) -> Error {
    assert(error_code >= 0);
    return Error{error_code};
}

template <typename T>
using Result = expected<T, Error>;

} // namespace uvio
