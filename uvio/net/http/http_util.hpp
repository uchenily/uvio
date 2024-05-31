#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace uvio::net::http {

class CaseInsensitiveEqual {
public:
    auto operator()(const std::string &left,
                    const std::string &right) const noexcept -> bool {
        return left.size() == right.size()
               && std::equal(left.begin(),
                             left.end(),
                             right.begin(),
                             [](char a, char b) {
                                 return tolower(a) == tolower(b);
                             });
    }
};

// Based on
// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash {
public:
    auto operator()(const std::string &str) const noexcept -> std::size_t {
        std::size_t    seed = 0;
        std::hash<int> hash;
        for (auto c : str) {
            seed ^= hash(tolower(c)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

using CaseInsensitiveMultimap = std::unordered_multimap<std::string,
                                                        std::string,
                                                        CaseInsensitiveHash,
                                                        CaseInsensitiveEqual>;

class HttpHeader {
public:
    auto add(const std::string &key, const std::string &value) {
        header_multimap_.emplace(key, value);
    }

    auto find(const std::string &case_insensitive_key)
        -> std::optional<std::string> {
        auto range = header_multimap_.equal_range(case_insensitive_key);
        if (range.first != header_multimap_.end()) {
            return range.first->second;
        } else {
            return std::nullopt;
        }
    }

    auto find_all(const std::string &case_insensitive_key)
        -> std::optional<std::vector<std::string>> {
        auto range = header_multimap_.equal_range(case_insensitive_key);
        if (range.first != header_multimap_.end()) {
            std::vector<std::string> res;
            for (auto it = range.first; it != range.second; ++it) {
                res.push_back(it->second);
            }
            return res;
        } else {
            return std::nullopt;
        }
    }

private:
    CaseInsensitiveMultimap header_multimap_;
};
} // namespace uvio::net::http
