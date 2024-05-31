#include "uvio/debug.hpp"
#include "uvio/net/http/http_util.hpp"

using namespace uvio::net::http;
auto main() -> int {
    CaseInsensitiveEqual case_insensitive_equal;
    ASSERT(case_insensitive_equal("Test", "tesT"));
    ASSERT(case_insensitive_equal("tesT", "test"));
    ASSERT(!case_insensitive_equal("test", "tset"));

    CaseInsensitiveHash case_insensitive_hash;
    ASSERT(case_insensitive_hash("Test") == case_insensitive_hash("tesT"));
    ASSERT(case_insensitive_hash("tesT") == case_insensitive_hash("test"));
    ASSERT(case_insensitive_hash("test") != case_insensitive_hash("tset"));

    HttpHeader header;
    header.add("Content-Type", "application/json");
    header.add("Set-Cookie", "ID=id");
    header.add("Set-Cookie", "name=name");
    ASSERT(header.find("content-type").has_value());
    ASSERT(!header.find("accept").has_value());
    ASSERT(header.find("Set-Cookie").has_value());

    auto real_value = *header.find_all("Set-Cookie");
    auto expected = std::vector<std::string>{"ID=id", "name=name"};
    ASSERT(real_value == expected);
}
