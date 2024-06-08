#pragma once

#include <array>
#include <string>

#if defined(HAS_SSL)
#include <openssl/buffer.h>
#include <openssl/evp.h>
#endif

class base64 {
public:
#if defined(HAS_SSL)
    static auto encode(std::string_view message) noexcept -> std::string {
        std::string base64;

        BIO     *bio = nullptr;
        BIO     *b64 = nullptr;
        BUF_MEM *bptr = BUF_MEM_new();

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new(BIO_s_mem());
        BIO_push(b64, bio);
        BIO_set_mem_buf(b64, bptr, BIO_CLOSE);

        // Write directly to base64-buffer to avoid copy
        auto base64_length = static_cast<std::size_t>(
            round(4 * ceil(static_cast<double>(message.size()) / 3.0)));
        base64.resize(base64_length);
        bptr->length = 0;
        bptr->max = base64_length + 1;
        bptr->data = base64.data();

        if (BIO_write(b64, message.data(), static_cast<int>(message.size()))
                <= 0
            || BIO_flush(b64) <= 0) {
            base64.clear();
        }

        // To keep &base64[0] through BIO_free_all(b64)
        bptr->length = 0;
        bptr->max = 0;
        bptr->data = nullptr;

        BIO_free_all(b64);

        return base64;
    }

    static auto decode(const std::string &base64) noexcept -> std::string {
        std::string ascii;

        // Resize ascii, however, the size is a up to two bytes too large.
        ascii.resize((6 * base64.size()) / 8);
        BIO *b64 = nullptr;
        BIO *bio = nullptr;

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new_mem_buf(base64.data(), static_cast<int>(base64.size()));
        bio = BIO_push(b64, bio);

        auto decoded_length
            = BIO_read(bio, ascii.data(), static_cast<int>(ascii.size()));
        if (decoded_length > 0) {
            ascii.resize(static_cast<std::size_t>(decoded_length));
        } else {
            ascii.clear();
        }

        BIO_free_all(b64);

        return ascii;
    }

#else

    static auto encode(std::string_view message) noexcept -> std::string {
        std::string ret;
        int         i = 0;
        int         j = 0;

        std::array<unsigned char, 3> buf3;
        std::array<unsigned char, 4> buf4;

        for (char ch : message) {
            buf3[i++] = ch;
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

    static auto decode(const std::string &base64) noexcept -> std::string {
        int         in_len = static_cast<int>(base64.size());
        std::string ret;

        int i = 0;
        int j = 0;
        int in = 0;

        std::array<char, 3> buf3;
        std::array<char, 4> buf4;

        while (((in_len--) != 0) && (base64[in] != '=')
               && is_base64(base64[in])) {
            buf4[i++] = base64[in];
            in++;
            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    buf4[i] = static_cast<char>(base64_chars.find(buf4[i]));
                }

                buf3[0] = (buf4[0] << 2) + ((buf4[1] & 0x30) >> 4);
                buf3[1] = ((buf4[1] & 0xf) << 4) + ((buf4[2] & 0x3c) >> 2);
                buf3[2] = ((buf4[2] & 0x3) << 6) + buf4[3];

                for (i = 0; i < 3; i++) {
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
                buf4[j] = static_cast<char>(base64_chars.find(buf4[j]));
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

private:
    static auto is_base64(unsigned char c) -> bool {
        return ((isalnum(c) != 0) || (c == '+') || (c == '/'));
    }

private:
    constexpr static const std::string_view base64_chars{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

#endif
};
