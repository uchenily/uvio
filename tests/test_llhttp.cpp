#include "uvio/debug.hpp"

#include "llhttp.h"

auto main() -> int {
    llhttp_t          parser;
    llhttp_settings_t settings;

    /*Initialize user callbacks and settings */
    llhttp_settings_init(&settings);

    /*Set user callback */
    settings.on_message_complete = [](llhttp_t * /*parser*/) -> int {
        LOG_INFO("Message completed!");
        return 0;
    };

    /*Initialize the parser in HTTP_BOTH mode, meaning that it will select
     *between HTTP_REQUEST and HTTP_RESPONSE parsing automatically while reading
     *the first input.
     */
    llhttp_init(&parser, HTTP_BOTH, &settings);

    /*Parse request! */
    std::string request
        = "POST / HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world";

    auto err = llhttp_execute(&parser, request.data(), request.size());
    if (err == HPE_OK) {
        LOG_INFO("Successfully parsed!");
    } else {
        LOG_INFO("Parse error: {} {}", llhttp_errno_name(err), parser.reason);
    }
}
