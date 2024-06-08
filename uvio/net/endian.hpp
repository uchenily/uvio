#pragma once

#include <string>

namespace uvio::net {

class byteorder {
public:
#if __BYTE_ORDER == __LITTLE_ENDIAN
    static inline auto htonll(uint64_t x) -> uint64_t {
        return __bswap_64(x);
    }
    static inline auto ntohll(uint64_t x) -> uint64_t {
        return __bswap_64(x);
    }
    static inline auto htonl(uint32_t x) -> uint32_t {
        return __bswap_32(x);
    }
    static inline auto ntohl(uint32_t x) -> uint32_t {
        return __bswap_32(x);
    }
    static inline auto htons(uint16_t x) -> uint16_t {
        return __bswap_16(x);
    }
    static inline auto ntohs(uint16_t x) -> uint16_t {
        return __bswap_16(x);
    }
#elif __BYTE_ORDER == __BIG_ENDIAN
    static inline auto htonll(uint64_t x) -> uint64_t {
        return x;
    }
    static inline auto ntohll(uint64_t x) -> uint64_t {
        return x;
    }
    static inline auto htonl(uint32_t x) -> uint32_t {
        return x;
    }
    static inline auto ntohl(uint32_t x) -> uint32_t {
        return x;
    }
    static inline auto htons(uint16_t x) -> uint16_t {
        return x;
    }
    static inline auto ntohs(uint16_t x) -> uint16_t {
        return x;
    }
#else
#error "__BYTE_ORDER not defined!"
#endif
};

} // namespace uvio::net
