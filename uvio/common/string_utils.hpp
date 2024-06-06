#pragma once
#include <string>
#include <string_view>

namespace uvio {

// NOTE: for debug only
inline auto make_visible(std::string_view input) -> std::string {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case ' ':
            result.append("␣");
            break;
        case '\t':
            result.append("\\t");
            break;
        case '\r':
            result.append("\\r");
            break;
        case '\n':
            result.append("\\n");
            break;
        default:
            // if (::isprint(c) != 0) {
            result.push_back(c);
            // }
        }
    }
    return result;
}

} // namespace uvio
