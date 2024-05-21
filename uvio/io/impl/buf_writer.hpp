#pragma once

#include "uvio/common/result.hpp"
#include "uvio/coroutine/task.hpp"
#include "uvio/macros.hpp"

namespace uvio::io::detail {

template <typename Derived>
class ImplBufWrite {
public:
    [[REMEMBER_CO_AWAIT]]
    auto write(std::span<const char> buf) -> Task<Result<std::size_t>> {
        if (static_cast<Derived *>(this)->w_stream_.w_remaining()
            >= static_cast<int>(buf.size())) {
            co_return static_cast<Derived *>(this)->w_stream_.read_from(buf);
        }

        Result<std::size_t> written_bytes{};
        if (static_cast<Derived *>(this)->w_stream_.r_remaining() > 0) {
            auto ret = co_await static_cast<Derived *>(this)->io_.write(
                static_cast<Derived *>(this)->w_stream_.r_slice());
            if (!ret) {
                co_return ret;
            }
            written_bytes.value()
                += static_cast<Derived *>(this)->w_stream_.r_remaining();
            static_cast<Derived *>(this)->w_stream_.r_increase(
                static_cast<Derived *>(this)->w_stream_.r_remaining());
            static_cast<Derived *>(this)->w_stream_.reset_data();

            auto ret2 = co_await static_cast<Derived *>(this)->io_.write(buf);
            if (!ret2) {
                co_return ret2;
            }
            written_bytes.value() += buf.size();

        } else {
            auto ret = co_await static_cast<Derived *>(this)->io_.write(buf);
            if (!ret) {
                co_return ret;
            }
            written_bytes.value() += buf.size();
        }

        co_return written_bytes;
    }

    [[REMEMBER_CO_AWAIT]]
    auto flush() -> Task<Result<void>> {
        while (!static_cast<Derived *>(this)->w_stream_.r_slice().empty()) {
            auto ret = co_await static_cast<Derived *>(this)->io_.write(
                static_cast<Derived *>(this)->w_stream_.r_slice());
            if (!ret) {
                co_return unexpected{ret.error()};
            }

            static_cast<Derived *>(this)->w_stream_.r_increase(
                static_cast<Derived *>(this)->w_stream_.r_remaining());
        }
        static_cast<Derived *>(this)->w_stream_.reset_pos();
        co_return Result<void>{};
    }

    [[REMEMBER_CO_AWAIT]]
    auto write_all(std::span<const char> buf) -> Task<Result<void>> {
        if (auto ret = co_await write(buf); !ret) {
            co_return unexpected{ret.error()};
        }
        if (auto ret = co_await flush(); !ret) {
            co_return unexpected{ret.error()};
        }
        co_return Result<void>{};
    }
};
} // namespace uvio::io::detail
