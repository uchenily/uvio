#pragma once

#if __cplusplus <= 202002L
#include "impl/expected.hpp"
// namespace uvio {
namespace std {
using tl::expected;
using tl::unexpected;
} // namespace std
#else
#include <expected>
// namespace uvio {
// using std::expected;
// using std::unexpected;
// } // namespace uvio
#endif
