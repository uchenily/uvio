#pragma once

#include <string>

namespace uvio::net {

#if defined(__linux__)
#define bswap_16 __builtin_bswap16
#define bswap_32 __builtin_bswap32
#define bswap_64 __builtin_bswap64
#elif defined(_WIN32)
#include <stdlib.h>
#include <winsock2.h>
#define bswap_16 _byteswap_ushort
#define bswap_32 _byteswap_ulong
#define bswap_64 _byteswap_uint64
#elif defined(__APPLE__)
#define bswap_16 OSSwapInt16
#define bswap_32 OSSwapInt32
#define bswap_64 OSSwapInt64
#else
static auto bswap_16(uint16_t x) -> uint16_t {
    return (((x >> 8) & 0xFF) | ((x & 0xFF) << 8));
}
static auto bswap_32(uint32_t x) -> uint32_t {
    return (((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8)
            | ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24));
}
static auto bswap_64(uint64_t x) -> uint64_t {
    return (((x & 0xFF00000000000000ULL) >> 56)
            | ((x & 0x00FF000000000000ULL) >> 40)
            | ((x & 0x0000FF0000000000ULL) >> 24)
            | ((x & 0x000000FF00000000ULL) >> 8)
            | ((x & 0x00000000FF000000ULL) << 8)
            | ((x & 0x0000000000FF0000ULL) << 24)
            | ((x & 0x000000000000FF00ULL) << 40)
            | ((x & 0x00000000000000FFULL) << 56));
}
#endif

class byteorder {
public:
    // 定义成 htonll()/htonl()/htons() 会出现宏命名冲突问题
#if __BYTE_ORDER == __LITTLE_ENDIAN
    static inline auto hton64(uint64_t x) -> uint64_t {
        return bswap_64(x);
    }
    static inline auto ntoh64(uint64_t x) -> uint64_t {
        return bswap_64(x);
    }
    static inline auto hton32(uint32_t x) -> uint32_t {
        return bswap_32(x);
    }
    static inline auto ntoh32(uint32_t x) -> uint32_t {
        return bswap_32(x);
    }
    static inline auto hton16(uint16_t x) -> uint16_t {
        return bswap_16(x);
    }
    static inline auto ntoh16(uint16_t x) -> uint16_t {
        return bswap_16(x);
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
#error "unknown byte order"
#endif

    static auto is_big_endian() -> bool {
#if defined(__linux__)
        return __BYTE_ORDER == __BIG_ENDIAN;
#elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86)             \
    || defined(_M_IA64) || defined(_M_X64)
        // known little architectures
        return false;
#else // unknown
        uint32_t x = 1;
        return reinterpret_cast<unsigned char &>(x) == 0;
#endif
    }
};

#if defined(bswap_16)
#undef bswap_64
#undef bswap_32
#undef bswap_16
#endif

} // namespace uvio::net
