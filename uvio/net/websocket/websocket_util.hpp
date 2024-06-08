#pragma once

#include <array>
#include <cctype>
#include <string>

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

static inline auto is_base64(unsigned char c) -> bool {
    return ((isalnum(c) != 0) || (c == '+') || (c == '/'));
}

inline auto base64_encode(unsigned char const *bytes_to_encode,
                          unsigned int         in_len) -> std::string {
    std::string ret;
    int         i = 0;
    int         j = 0;

    std::array<unsigned char, 3> buf3;
    std::array<unsigned char, 4> buf4;

    while ((in_len--) != 0u) {
        buf3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                ret += base64_chars[buf4[i]];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 3; j++) {
            buf3[j] = '\0';
        }

        buf4[0] = (buf3[0] & 0xfc) >> 2;
        buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
        buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
        buf4[3] = buf3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            ret += base64_chars[buf4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

inline auto base64_decode(std::string const &encoded_string) -> std::string {
    int         in_len = static_cast<int>(encoded_string.size());
    int         i = 0;
    int         j = 0;
    int         in = 0;
    std::string ret;

    std::array<unsigned char, 3> buf3;
    std::array<unsigned char, 4> buf4;

    while (((in_len--) != 0) && (encoded_string[in] != '=')
           && is_base64(encoded_string[in])) {
        buf4[i++] = encoded_string[in];
        in++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                buf4[i]
                    = static_cast<unsigned char>(base64_chars.find(buf4[i]));
            }

            buf3[0] = (buf4[0] << 2) + ((buf4[1] & 0x30) >> 4);
            buf3[1] = ((buf4[1] & 0xf) << 4) + ((buf4[2] & 0x3c) >> 2);
            buf3[2] = ((buf4[2] & 0x3) << 6) + buf4[3];

            for (i = 0; (i < 3); i++) {
                ret += buf3[i];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 4; j++) {
            buf4[j] = 0;
        }

        for (j = 0; j < 4; j++) {
            buf4[j] = static_cast<unsigned char>(base64_chars.find(buf4[j]));
        }

        buf3[0] = (buf4[0] << 2) + ((buf4[1] & 0x30) >> 4);
        buf3[1] = ((buf4[1] & 0xf) << 4) + ((buf4[2] & 0x3c) >> 2);
        buf3[2] = ((buf4[2] & 0x3) << 6) + buf4[3];

        for (j = 0; (j < i - 1); j++) {
            ret += buf3[j];
        }
    }

    return ret;
}
