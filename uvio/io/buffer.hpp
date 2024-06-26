#pragma once

#include "uvio/common/string_utils.hpp"
#include "uvio/debug.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

#include <cassert>

namespace uvio::io::detail {
///             r_begin      r_end/w_begin      w_end
/// +-------------+---------------+---------------+
/// |             |     r_slice   |    w_slice    |
/// +-------------+---------------+---------------+
/// 0    <=     r_pos_   <=     w_pos_    <=     size
///               |<-r_remaining->|<-w_remaining->|

class StreamBuffer {
public:
    StreamBuffer(std::size_t size = DEFAULT_BUF_SIZE)
        : buf_(size) {}

    auto r_remaining() const noexcept -> int {
        return w_pos_ - r_pos_;
    }

    auto r_begin() const noexcept -> std::vector<char>::const_iterator {
        return buf_.begin() + r_pos_;
    }

    auto r_end() const noexcept -> std::vector<char>::const_iterator {
        return r_begin() + r_remaining();
    }

    auto r_slice() const noexcept -> std::span<const char> {
        return {r_begin(), r_end()};
    }

    auto w_remaining() const noexcept -> int {
        return static_cast<int>(buf_.size()) - w_pos_;
    }

    auto w_begin() noexcept -> std::vector<char>::iterator {
        return buf_.begin() + w_pos_;
    }

    auto w_end() noexcept -> std::vector<char>::iterator {
        return w_begin() + w_remaining();
    }

    auto w_slice() noexcept -> std::span<char> {
        return {w_begin(), w_end()};
    }

    auto empty() const noexcept -> bool {
        return r_pos_ == w_pos_;
    }

    auto capacity() const noexcept -> std::size_t {
        return buf_.size();
    }

    auto write_to(std::span<char> dst) noexcept -> std::size_t {
        auto len = (std::min)(static_cast<std::size_t>(r_remaining()),
                              dst.size_bytes());
        std::copy_n(r_begin(), len, dst.begin());
        r_increase(len);
        if (r_pos_ == w_pos_) {
            reset_pos();
        }
        return len;
    }

    auto read_from(std::span<const char> src) noexcept -> std::size_t {
        auto len = (std::min)(static_cast<std::size_t>(w_remaining()),
                              src.size_bytes());
        std::copy_n(src.begin(), len, w_begin());
        w_increase(len);
        return len;
    }

    auto find_flag_and_return_slice(std::string_view end_str) const noexcept
        -> std::span<const char> {
        LOG_DEBUG(
            "r_slice(): `{}` ({} bytes)",
            make_visible(std::string_view{r_slice().data(), r_slice().size()}),
            r_slice().size_bytes());
        auto pos = std::string_view{r_slice().data()}.find(end_str);
        if (pos == std::string_view::npos) {
            LOG_DEBUG("not found: `{}` ({} bytes)",
                      make_visible(end_str),
                      end_str.size());
            return {};
        } else {
            return {r_begin(), pos + end_str.size()};
        }
    }

    auto find_flag_and_return_slice(char end_char) const noexcept
        -> std::span<const char> {
        auto pos = std::string_view{r_slice().data()}.find(end_char);
        if (pos == std::string_view::npos) {
            return {};
        } else {
            return {r_begin(), pos + 1};
        }
    }

    auto disable() noexcept -> void {
        // buf_.clear();
        // better
        // std::array<char, SIZE>().swap(buf_);
    }

    // private:
    auto reset_pos() -> void {
        r_pos_ = w_pos_ = 0;
    }

    void reset_data() noexcept {
        auto len = r_remaining();
        std::copy(r_begin(), r_end(), buf_.begin());
        r_pos_ = 0;
        w_pos_ = len;
    }

    auto r_increase(std::size_t n) noexcept -> void {
        r_pos_ += static_cast<int>(n);
        assert(r_pos_ <= w_pos_);
    }

    auto w_increase(std::size_t n) noexcept -> void {
        w_pos_ += static_cast<int>(n);
        assert(r_pos_ <= w_pos_);
    }

public:
    constexpr static std::size_t DEFAULT_BUF_SIZE{
        static_cast<std::size_t>(8 * 1024)};

private:
private:
    std::vector<char> buf_;
    int               r_pos_{0};
    int               w_pos_{0};
};

} // namespace uvio::io::detail
