#pragma once

#include "uvio/common/result.hpp"
#include "uvio/core.hpp"

namespace uvio::codec {

template <typename Derived>
class Codec {
public:
    template <typename Ret, typename Output, typename Reader>
    auto Decode(Output &output, Reader &reader) -> Task<Result<void>> {
        co_return co_await static_cast<Derived *>(this)->decode(output, reader);
    }

    template <typename Ret, typename Input, typename Writer>
    auto Encode(Input input, Writer &writer) -> Task<Result<Ret>> {
        co_return co_await static_cast<Derived *>(this)->encode(input, writer);
    }
};
} // namespace uvio::codec
