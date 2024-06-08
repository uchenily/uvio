#pragma once

#include "uvio/common/result.hpp"
#include "uvio/core.hpp"

namespace uvio::codec {

template <typename Derived>
class Codec {
public:
    template <typename Ret, typename Reader>
    auto Decode(Reader &reader) -> Task<Result<Ret>> {
        co_return co_await static_cast<Derived *>(this)->decode(reader);
    }

    template <typename Ret, typename Arg, typename Writer>
    auto Encode(Arg arg, Writer &writer) -> Task<Result<Ret>> {
        co_return co_await static_cast<Derived *>(this)->encode(arg, writer);
    }
};
} // namespace uvio::codec
