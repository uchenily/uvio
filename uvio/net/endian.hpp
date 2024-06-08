#pragma once

#include <string>

namespace uvio::net {

class byteorder {
public:
    // 定义成 htonll()/htonl()/htons() 会出现宏命名冲突问题
#if __BYTE_ORDER == __LITTLE_ENDIAN
    static inline auto hton64(uint64_t x) -> uint64_t {
        return __bswap_64(x);
    }
    static inline auto ntoh64(uint64_t x) -> uint64_t {
        return __bswap_64(x);
    }
    static inline auto hton32(uint32_t x) -> uint32_t {
        return __bswap_32(x);
    }
    static inline auto ntoh32(uint32_t x) -> uint32_t {
        return __bswap_32(x);
    }
    static inline auto hton16(uint16_t x) -> uint16_t {
        return __bswap_16(x);
    }
    static inline auto ntoh16(uint16_t x) -> uint16_t {
        return __bswap_16(x);
    }
#elif __BYTE_ORDER == __BIG_ENDIAN
    static inline auto hton64(uint64_t x) -> uint64_t {
        return x;
    }
    static inline auto ntoh64(uint64_t x) -> uint64_t {
        return x;
    }
    static inline auto hton32(uint32_t x) -> uint32_t {
        return x;
    }
    static inline auto ntoh32(uint32_t x) -> uint32_t {
        return x;
    }
    static inline auto hton16(uint16_t x) -> uint16_t {
        return x;
    }
    static inline auto ntoh16(uint16_t x) -> uint16_t {
        return x;
    }
#else
#error "__BYTE_ORDER not defined!"
#endif
};

} // namespace uvio::net
