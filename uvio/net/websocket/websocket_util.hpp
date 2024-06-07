#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

static inline auto is_base64(unsigned char c) -> bool {
    return ((isalnum(c) != 0) || (c == '+') || (c == '/'));
}

inline auto base64_encode(unsigned char const *bytes_to_encode,
                          unsigned int         in_len) -> std::string {
    std::string   ret;
    int           i = 0;
    int           j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while ((in_len--) != 0u) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4)
                              + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2)
                              + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++) {
                ret += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1]
            = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2]
            = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) {
            ret += base64_chars[char_array_4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

inline auto base64_decode(std::string const &encoded_string) -> std::string {
    int           in_len = static_cast<int>(encoded_string.size());
    int           i = 0;
    int           j = 0;
    int           in_ = 0;
    unsigned char char_array_4[4];
    unsigned char char_array_3[3];
    std::string   ret;

    while (((in_len--) != 0) && (encoded_string[in_] != '=')
           && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = static_cast<unsigned char>(
                    base64_chars.find(char_array_4[i]));
            }

            char_array_3[0]
                = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4)
                              + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret += char_array_3[i];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        for (j = 0; j < 4; j++) {
            char_array_4[j] = static_cast<unsigned char>(
                base64_chars.find(char_array_4[j]));
        }

        char_array_3[0]
            = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1]
            = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret += char_array_3[j];
        }
    }

    return ret;
}

// Ported from https://github.com/darrenjs/openssl_examples
// MIT licensed
class TLS {
    enum SSLStatus { SSLSTATUS_OK, SSLSTATUS_WANT_IO, SSLSTATUS_FAIL };

public:
    TLS(SSL_CTX *ctx, bool server = true, const char *hostname = nullptr)
        : m_SSL(SSL_new(ctx))
        , m_ReadBIO(BIO_new(BIO_s_mem()))
        , m_WriteBIO(BIO_new(BIO_s_mem())) {

        if (server) {
            SSL_set_accept_state(m_SSL);
        } else {
            SSL_set_connect_state(m_SSL);
        }

        if (!server && hostname) {
            SSL_set_tlsext_host_name(m_SSL, hostname);
        }
        SSL_set_bio(m_SSL, m_ReadBIO, m_WriteBIO);

        if (!server) {
            DoSSLHandhake();
        }
    }

    ~TLS() {
        SSL_free(m_SSL);
    }

    TLS(const TLS &other) = delete;
    auto operator=(const TLS &other) -> TLS & = delete;

    // Helper to setup SSL, you still need to create the context
    static void InitSSL() {
        static std::once_flag f;
        std::call_once(f, []() {
            SSL_library_init();
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER < 0x30000000L || defined(LIBRESSL_VERSION_NUMBER)
            ERR_load_BIO_strings();
#endif
            ERR_load_crypto_strings();
        });
    }

    // Writes unencrypted bytes to be encrypted and sent out
    // If this returns false, the connection must be closed
    auto Write(const char *buf, size_t len) -> bool {
        m_EncryptBuf.insert(m_EncryptBuf.end(), buf, buf + len);
        return DoEncrypt();
    }

    // Process raw bytes received from the other side
    // If this returns false, the connection must be closed
    template <typename F>
    auto ReceivedData(const char *src, size_t len, const F &f) -> bool {
        int n = 0;
        while (len > 0) {
            n = BIO_write(m_ReadBIO, src, len);

            // Assume bio write failure is unrecoverable
            if (n <= 0) {
                return false;
            }

            src += n;
            len -= n;

            if (!SSL_is_init_finished(m_SSL)) {
                if (DoSSLHandhake() == SSLSTATUS_FAIL) {
                    return false;
                }
                if (!SSL_is_init_finished(m_SSL)) {
                    return true;
                }
            }

            ERR_clear_error();
            do {
                char buf[4096];
                n = SSL_read(m_SSL, buf, sizeof buf);
                if (n > 0) {
                    f(buf, static_cast<size_t>(n));
                }
            } while (n > 0);

            auto status = GetSSLStatus(n);
            if (status == SSLSTATUS_WANT_IO) {
                do {
                    char buf[4096];
                    n = BIO_read(m_WriteBIO, buf, sizeof(buf));
                    if (n > 0) {
                        QueueEncrypted(buf, n);
                    } else if (!BIO_should_retry(m_WriteBIO)) {
                        return false;
                    }
                } while (n > 0);
            } else if (status == SSLSTATUS_FAIL) {
                return false;
            }
        }

        return true;
    }

    template <typename F>
    void ForEachPendingWrite(const F &f) {
        // If the callback does something crazy like calling Write inside of it
        // We need to handle this carefully, thus the swap.
        for (;;) {
            if (m_WriteBuf.empty()) {
                return;
            }

            std::vector<char> buf;
            std::swap(buf, m_WriteBuf);

            f(buf.data(), buf.size());
        }
    }

    auto IsHandshakeFinished() -> bool {
        return SSL_is_init_finished(m_SSL) != 0;
    }

private:
    auto GetSSLStatus(int n) -> SSLStatus {
        switch (SSL_get_error(m_SSL, n)) {
        case SSL_ERROR_NONE:
            return SSLSTATUS_OK;

        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_READ:
            return SSLSTATUS_WANT_IO;

        case SSL_ERROR_ZERO_RETURN:
        case SSL_ERROR_SYSCALL:
        default:
            return SSLSTATUS_FAIL;
        }
    }

    void QueueEncrypted(const char *buf, size_t len) {
        m_WriteBuf.insert(m_WriteBuf.end(), buf, buf + len);
    }

    auto DoEncrypt() -> bool {
        if (SSL_is_init_finished(m_SSL) == 0) {
            return true;
        }

        int n = 0;

        while (!m_EncryptBuf.empty()) {
            ERR_clear_error();
            n = SSL_write(m_SSL,
                          m_EncryptBuf.data(),
                          static_cast<int>(m_EncryptBuf.size()));

            if (GetSSLStatus(n) == SSLSTATUS_FAIL) {
                return false;
            }

            if (n > 0) {
                // Consume bytes
                m_EncryptBuf.erase(m_EncryptBuf.begin(),
                                   m_EncryptBuf.begin() + n);

                // Write them out
                do {
                    char buf[4096];
                    n = BIO_read(m_WriteBIO, buf, sizeof buf);
                    if (n > 0) {
                        QueueEncrypted(buf, n);
                    } else if (!BIO_should_retry(m_WriteBIO)) {
                        return false;
                    }
                } while (n > 0);
            }
        }

        return true;
    }

    auto DoSSLHandhake() -> SSLStatus {
        ERR_clear_error();
        SSLStatus const status = GetSSLStatus(SSL_do_handshake(m_SSL));

        // Did SSL request to write bytes?
        if (status == SSLSTATUS_WANT_IO) {
            int n = 0;
            do {
                char buf[4096];
                n = BIO_read(m_WriteBIO, buf, sizeof buf);

                if (n > 0) {
                    QueueEncrypted(buf, n);
                } else if (!BIO_should_retry(m_WriteBIO)) {
                    return SSLSTATUS_FAIL;
                }

            } while (n > 0);
        }

        return status;
    }

    std::vector<char> m_EncryptBuf; // Bytes waiting to be encrypted
    std::vector<char> m_WriteBuf;   // Bytes waiting to be written to the socket

    SSL *m_SSL;
    BIO *m_ReadBIO;
    BIO *m_WriteBIO;
};
