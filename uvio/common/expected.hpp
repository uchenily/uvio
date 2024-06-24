#pragma once

#if __cplusplus <= 202002L
#include "uvio/common/impl/expected.hpp"
namespace uvio {
using tl::expected;
using tl::unexpected;
} // namespace uvio
#else
#include <expected>
namespace uvio {
using std::expected;
using std::unexpected;
} // namespace uvio
#endif
