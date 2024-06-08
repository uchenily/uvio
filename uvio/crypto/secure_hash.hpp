#pragma once

#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/evp.h>
#include <openssl/sha.h>

class secure_hash {
public:
    // Return hex string from bytes in input string
    static auto to_hex_string(const std::string &input) noexcept
        -> std::string {
        std::stringstream hex_stream;
        hex_stream << std::hex << std::internal << std::setfill('0');
        for (auto &byte : input) {
            hex_stream << std::setw(2)
                       << static_cast<int>(static_cast<unsigned char>(byte));
        }
        return hex_stream.str();
    }

    static auto sha1(const std::string &input,
                     std::size_t iterations = 1) noexcept -> std::string {
        std::string hash;

        hash.resize(20);
        SHA1(reinterpret_cast<const unsigned char *>(input.data()),
             input.size(),
             reinterpret_cast<unsigned char *>(hash.data()));

        for (std::size_t c = 1; c < iterations; ++c) {
            SHA1(reinterpret_cast<const unsigned char *>(hash.data()),
                 hash.size(),
                 reinterpret_cast<unsigned char *>(hash.data()));
        }

        return hash;
    }

    static auto sha256(const std::string &input,
                       std::size_t iterations = 1) noexcept -> std::string {
        std::string hash;

        hash.resize(256 / 8);
        SHA256(reinterpret_cast<const unsigned char *>(input.data()),
               input.size(),
               reinterpret_cast<unsigned char *>(hash.data()));

        for (std::size_t c = 1; c < iterations; ++c) {
            SHA256(reinterpret_cast<const unsigned char *>(hash.data()),
                   hash.size(),
                   reinterpret_cast<unsigned char *>(hash.data()));
        }

        return hash;
    }

    static auto sha512(const std::string &input,
                       std::size_t iterations = 1) noexcept -> std::string {
        std::string hash;

        hash.resize(512 / 8);
        SHA512(reinterpret_cast<const unsigned char *>(input.data()),
               input.size(),
               reinterpret_cast<unsigned char *>(hash.data()));

        for (std::size_t c = 1; c < iterations; ++c) {
            SHA512(reinterpret_cast<const unsigned char *>(hash.data()),
                   hash.size(),
                   reinterpret_cast<unsigned char *>(hash.data()));
        }

        return hash;
    }

    // key_size is number of bytes of the returned key
    static auto pbkdf2(const std::string &password,
                       const std::string &salt,
                       int                iterations,
                       int                key_size) noexcept -> std::string {
        std::string key;
        key.resize(static_cast<std::size_t>(key_size));
        PKCS5_PBKDF2_HMAC_SHA1(
            password.c_str(),
            static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char *>(salt.c_str()),
            static_cast<int>(salt.size()),
            iterations,
            key_size,
            reinterpret_cast<unsigned char *>(key.data()));
        return key;
    }
};
