#pragma once

#include "uvio/codec.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net.hpp"
#include "uvio/net/http/http_protocol.hpp"
#include "uvio/net/http/http_util.hpp"

namespace uvio::net::http {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

class HttpCodec : public Codec<HttpCodec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<std::string>> {
        HttpRequest req;
        std::string request_line;
        if (auto ret = co_await reader.read_until(request_line, "\r\n"); !ret) {
            co_return unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_line);
        // GET / HTTP/1.1
        auto ret = parse_request_line(request_line);
        if (!ret) {
            co_return unexpected{ret.error()};
        }
        auto [method, uri, version] = ret.value();
        LOG_DEBUG("method: {}", method);
        LOG_DEBUG("uri: {}", uri);
        LOG_DEBUG("version: {}", version);

        std::string request_headers;
        if (auto ret = co_await reader.read_until(request_headers, "\r\n\r\n");
            !ret) {
            co_return unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_headers);
        // Host: xxx
        // Content-Type: application/html
        auto headers = parse_headers(request_headers);

        std::string body;

        if (auto has_length = headers.find("Content-Length")) {
            auto length = std::stoi(has_length.value());
            LOG_DEBUG("Parsed body length: {}", length);
            body.resize(length);
            if (auto ret = co_await reader.read_exact(body); !ret) {
                co_return unexpected{ret.error()};
            }
            LOG_DEBUG("body: `{}`", body);
        }

        co_return body;
    }

    template <typename Writer>
    auto encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        LOG_DEBUG("http response encoding ...");
        if (auto ret = co_await writer.write(
                std::format("HTTP/1.0 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                            message.size(),
                            std::string_view{message.data(), message.size()}));
            !ret) {
            co_return unexpected{ret.error()};
        }
        if (auto ret = co_await writer.flush(); !ret) {
            co_return ret;
        }

        co_return Result<void>{};
    }

    auto parse_request_line(std::string_view line) -> Result<
        std::tuple<std::string_view, std::string_view, std::string_view>> {
        std::vector<std::string_view> parts;

        std::size_t start = 0;
        std::size_t end = 0;

        // first space
        while (end < line.size() && line[end] != ' ') {
            ++end;
        }
        if (line[end] != ' ') {
            return unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // second space
        while (end < line.size() && line[end] != ' ') {
            ++end;
        }
        if (line[end] != ' ') {
            return unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // HTTP version
        while (end < line.size() && line[end] != '\r' && line[end] != '\n') {
            ++end;
        }
        if (line[end] != '\r' && line[end] != '\n') {
            return unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);

        return std::tuple{parts[0], parts[1], parts[2]};
    }

    auto trim(std::string_view str) -> std::string_view {
        std::size_t start = 0;
        std::size_t end = str.size();

        while (start < end && std::isspace(str[start]) != 0) {
            ++start;
        }
        while (end > start && std::isspace(str[end - 1]) != 0) {
            --end;
        }

        return str.substr(start, end - start);
    }

    auto parse_headers(std::string_view headers) -> HttpHeader {
        HttpHeader  header;
        std::size_t start = 0;
        std::size_t end = 0;
        while (end < headers.size()) {
            while (end < headers.size() && headers[end] != '\r'
                   && headers[end] != '\n') {
                ++end;
            }

            // get key-value pair
            auto colon_pos = headers.find(':', start);
            if (colon_pos != std::string_view::npos) {
                auto key = headers.substr(start, colon_pos - start);
                auto value = headers.substr(colon_pos + 1, end - colon_pos - 1);
                key = trim(key);
                value = trim(value);
                header.add(std::string{key}, std::string{value});
            }

            // skip next "\r\n"
            while (end < headers.size()
                   && (headers[end] == '\r' || headers[end] == '\n')) {
                ++end;
            }
            // if (end + 1 < headers.size() && headers[end] == '\r'
            //     && headers[end + 1] == '\n') {
            //     end += 2;
            // } else {
            //     ++end;
            // }

            start = end;
        }
        return header;
    }
};

class HttpFramed {
public:
    explicit HttpFramed(TcpStream &&stream) {
        auto [reader, writer] = std::move(stream).into_split();
        buffered_reader_ = BufferedReader{std::move(reader), 1024};
        buffered_writer_ = BufferedWriter{std::move(writer), 1024};
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto write_response(HttpResponse resp) -> Task<Result<void>> {
        co_return co_await codec_.Encode(resp.body, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<HttpRequest>> {
        auto result = co_await codec_.Decode(buffered_reader_);
        // FIXME: Decode() -> HttpRequest?
        co_return HttpRequest{.url = "/", .body = result.value()};
    }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr},
                                    1024};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr},
                                    1024};
    HttpCodec      codec_{};
};
} // namespace uvio::net::http
