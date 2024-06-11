#pragma once

#include "uvio/codec/base.hpp"
#include "uvio/net/http/http_protocol.hpp"

namespace uvio::codec {

using namespace uvio::net;

class HttpCodec : public Codec<HttpCodec> {
public:
    template <typename Reader>
    auto decode(http::HttpRequest &req, Reader &reader) -> Task<Result<void>> {
        std::string request_line;
        if (auto ret = co_await reader.read_until(request_line, "\r\n"); !ret) {
            co_return std::unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_line);
        // GET / HTTP/1.1
        auto ret = parse_request_line(request_line);
        if (!ret) {
            co_return std::unexpected{ret.error()};
        }
        auto [method, uri, version] = ret.value();
        LOG_DEBUG("method: {}", method);
        LOG_DEBUG("uri: {}", uri);
        LOG_DEBUG("version: {}", version);
        req.method = method;
        req.uri = uri;

        std::string request_headers(2, 0);
        if (auto ret = co_await reader.read_exact(request_headers); !ret) {
            co_return std::unexpected{ret.error()};
        } else if (request_headers == "\r\n") {
            // no request_headers
            co_return Result<void>{};
        }

        if (auto ret = co_await reader.read_until(request_headers, "\r\n\r\n");
            !ret) {
            co_return std::unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_headers);
        // Host: xxx
        // Content-Type: application/html
        auto headers = parse_headers(request_headers);
        req.headers = std::move(headers);

        if (auto has_length = headers.find("Content-Length")) {
            std::string body;
            auto        length = std::stoi(has_length.value());
            LOG_DEBUG("Parsed body length: {}", length);
            body.resize(length);
            if (auto ret = co_await reader.read_exact(body); !ret) {
                co_return std::unexpected{ret.error()};
            }
            LOG_DEBUG("body: `{}`", body);
            req.body = std::move(body);
        }

        co_return Result<void>{};
    }

    template <typename Writer>
    auto encode(http::HttpResponse &resp, Writer &writer)
        -> Task<Result<void>> {
        if (!resp.headers.find("Upgrade")) {
            co_return co_await http_response(resp, writer);
        }

        co_return co_await websocket_handshake(resp, writer);
    }

    // client
    template <typename Reader>
    auto decode(http::HttpResponse &resp, Reader &reader)
        -> Task<Result<void>> {
        std::string status_line;
        if (auto ret = co_await reader.read_until(status_line, "\r\n"); !ret) {
            co_return std::unexpected{ret.error()};
        }
        LOG_DEBUG("{}", status_line);
        // HTTP/1.1 200 OK
        auto ret = parse_status_line(status_line);
        if (!ret) {
            co_return std::unexpected{ret.error()};
        }
        auto [version, status_code, status_text] = ret.value();
        LOG_DEBUG("version: {}", version);
        LOG_DEBUG("status_code: {}", status_code);
        LOG_DEBUG("status_text: {}", status_text);
        resp.status_code = std::stoi(std::string{status_code});

        std::string request_headers(2, 0);
        if (auto ret = co_await reader.read_exact(request_headers); !ret) {
            co_return std::unexpected{ret.error()};
        } else if (request_headers == "\r\n") {
            // no response body
            co_return Result<void>{};
        }

        if (auto ret = co_await reader.read_until(request_headers, "\r\n\r\n");
            !ret) {
            co_return std::unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_headers);
        // Server: Apache
        // Content-Length: 81
        // Connection: Keep-Alive
        // Content-Type: text/html
        auto headers = parse_headers(request_headers);
        resp.headers = std::move(headers);

        if (auto has_length = headers.find("Content-Length")) {
            std::string body;
            auto        length = std::stoi(has_length.value());
            LOG_DEBUG("Parsed body length: {}", length);
            body.resize(length);
            if (auto ret = co_await reader.read_exact(body); !ret) {
                co_return std::unexpected{ret.error()};
            }
            LOG_DEBUG("body: `{}`", body);
            resp.body = std::move(body);
        }

        co_return Result<void>{};
    }

    // client
    template <typename Writer>
    auto encode(http::HttpRequest &req, Writer &writer) -> Task<Result<void>> {
        co_await writer.write(
            std::format("{} {} HTTP/1.1\r\n", req.method, req.uri));
        for (auto &[k, v] : req.headers.inner()) {
            co_await writer.write(std::format("{}: {}\r\n", k, v));
        }
        co_await writer.write(std::string_view{"\r\n"});

        if (auto ret = co_await writer.flush(); !ret) {
            co_return ret;
        }
        co_return Result<void>{};
    }

    template <typename Writer>
    auto http_response(http::HttpResponse &resp, Writer &writer)
        -> Task<Result<void>> {
        // 对普通的 HTTP 请求的响应
        if (auto ret = co_await writer.write(
                std::format("HTTP/1.0 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                            resp.body.size(),
                            resp.body));
            !ret) {
            co_return std::unexpected{ret.error()};
        }
        if (auto ret = co_await writer.flush(); !ret) {
            co_return ret;
        }
        co_return Result<void>{};
    }

    template <typename Writer>
    auto websocket_handshake(http::HttpResponse &resp, Writer &writer)
        -> Task<Result<void>> {
        // 这里有个比较坑的地方, 如果写入的类型是 char *,
        // 那么长度会加一(结尾的\0), '\0'也会写入 buf 导致解析出问题 所以要使用
        // std::string/std::string_view, 避免使用c字符串
        co_await writer.write(
            std::string_view{"HTTP/1.1 101 Switching Protocols\r\n"});
        for (auto &[k, v] : resp.headers.inner()) {
            co_await writer.write(std::format("{}: {}\r\n", k, v));
        }
        co_await writer.write(std::string_view{"\r\n"});

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
            return std::unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // second space
        while (end < line.size() && line[end] != ' ') {
            ++end;
        }
        if (line[end] != ' ') {
            return std::unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // HTTP version
        while (end < line.size() && line[end] != '\r' && line[end] != '\n') {
            ++end;
        }
        if (line[end] != '\r' && line[end] != '\n') {
            return std::unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);

        return std::tuple{parts[0], parts[1], parts[2]};
    }

    auto parse_status_line(std::string_view line) -> Result<
        std::tuple<std::string_view, std::string_view, std::string_view>> {
        std::vector<std::string_view> parts;

        std::size_t start = 0;
        std::size_t end = 0;

        // HTTP version
        while (end < line.size() && line[end] != ' ') {
            ++end;
        }
        if (line[end] != ' ') {
            return std::unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // status code
        while (end < line.size() && line[end] != ' ') {
            ++end;
        }
        if (line[end] != ' ') {
            return std::unexpected{make_uvio_error(Error::Unclassified)};
        }
        parts.emplace_back(line.data() + start, end - start);
        start = ++end;

        // status text
        while (end < line.size() && line[end] != '\r' && line[end] != '\n') {
            ++end;
        }
        if (line[end] != '\r' && line[end] != '\n') {
            return std::unexpected{make_uvio_error(Error::Unclassified)};
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

    auto parse_headers(std::string_view headers) -> http::HttpHeader {
        http::HttpHeader header;
        std::size_t      start = 0;
        std::size_t      end = 0;
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

} // namespace uvio::codec
