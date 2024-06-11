#pragma once

#include "uvio/common/result.hpp"
#include "uvio/coroutine/task.hpp"
#include "uvio/macros.hpp"

namespace uvio::io::detail {

template <typename Derived>
class ImplBufRead {
public:
    [[REMEMBER_CO_AWAIT]]
    auto read(std::span<char> buf) noexcept -> Task<Result<std::size_t>> {
        // When buf is large enough, it's not necessory for additional copy from
        // StreamBuffer to buf
        if (static_cast<Derived *>(this)->r_stream_.capacity()
            < buf.size_bytes()) {
            auto len = static_cast<Derived *>(this)->r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret = co_await static_cast<Derived *>(this)->io_.read(buf);
            if (!ret) {
                co_return ret;
            }
            ret.value() += len;
            co_return ret;
        }

        if (!static_cast<Derived *>(this)->r_stream_.empty()) {
            co_return static_cast<Derived *>(this)->r_stream_.write_to(buf);
        }

        while (true) {
            // TODO(x)
            // if (static_cast<Derived *>(this)->r_stream_.r_remaining() +
            // static_cast<Derived *>(this)->r_stream_.w_remaining()
            if (static_cast<Derived *>(this)->r_stream_.w_remaining()
                < static_cast<int>(buf.size_bytes())) {
                static_cast<Derived *>(this)->r_stream_.reset_data();
            }

            auto ret = co_await static_cast<Derived *>(this)->io_.read(
                static_cast<Derived *>(this)->r_stream_.w_slice());
            if (!ret) {
                co_return ret;
            }
            static_cast<Derived *>(this)->r_stream_.w_increase(ret.value());

            if (!static_cast<Derived *>(this)->r_stream_.empty()
                || ret.value() == 0) {
                co_return static_cast<Derived *>(this)->r_stream_.write_to(buf);
            }
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_exact(std::span<char> buf) noexcept -> Task<Result<void>> {
        if (static_cast<Derived *>(this)->r_stream_.capacity()
            < buf.size_bytes()) {
            auto len = static_cast<Derived *>(this)->r_stream_.write_to(buf);
            buf = buf.subspan(len, buf.size_bytes() - len);
            auto ret
                = co_await static_cast<Derived *>(this)->io_.read_exact(buf);
            if (!ret) {
                co_return ret;
            }
            co_return Result<void>{};
        }

        auto exact_bytes = static_cast<int>(buf.size_bytes());

        if (static_cast<Derived *>(this)->r_stream_.r_remaining()
            >= exact_bytes) {
            static_cast<Derived *>(this)->r_stream_.write_to(buf);
            co_return Result<void>{};
        }

        while (true) {
            if (static_cast<Derived *>(this)->r_stream_.w_remaining()
                < exact_bytes) {
                static_cast<Derived *>(this)->r_stream_.reset_data();
            }

            auto ret = co_await static_cast<Derived *>(this)->io_.read(
                static_cast<Derived *>(this)->r_stream_.w_slice());
            if (!ret) {
                co_return std::unexpected{ret.error()};
            }
            static_cast<Derived *>(this)->r_stream_.w_increase(ret.value());

            if (static_cast<Derived *>(this)->r_stream_.r_remaining()
                >= exact_bytes) {
                static_cast<Derived *>(this)->r_stream_.write_to(buf);
                co_return Result<void>{};
            }
        }
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_until(std::string &buf, std::string_view end_flag) noexcept
        -> Task<Result<std::size_t>> {
        Result<std::size_t> ret;
        while (true) {
            if (auto slice
                = static_cast<Derived *>(this)
                      ->r_stream_.find_flag_and_return_slice(end_flag);
                !slice.empty()) {
                buf.append(slice.begin(), slice.end());
                static_cast<Derived *>(this)->r_stream_.r_increase(
                    slice.size_bytes());
                break;
            } else {
                // auto readable
                //     = static_cast<Derived *>(this)->r_stream_.r_slice();
                // buf.append(readable.begin(), readable.end());
                // static_cast<Derived *>(this)->r_stream_.reset_pos();
            }

            ret = co_await static_cast<Derived *>(this)->io_.read(
                static_cast<Derived *>(this)->r_stream_.w_slice());
            if (!ret) {
                co_return std::unexpected{ret.error()};
            }
            // if (ret.value() == 0) {
            //     break;
            // }
            static_cast<Derived *>(this)->r_stream_.w_increase(ret.value());
        }

        co_return buf.size();
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_line(std::string &buf) noexcept -> Task<Result<std::size_t>> {
        return read_until(buf, "\n");
    }
};
} // namespace uvio::io::detail
