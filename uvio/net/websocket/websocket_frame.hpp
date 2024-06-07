#pragma once

#include "uvio/codec.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include "uvio/common/string_utils.hpp"
#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net.hpp"
#include "uvio/net/http/http_util.hpp"
#include "uvio/net/websocket/websocket_protocol.hpp"
#include "uvio/net/websocket/websocket_util.hpp"

namespace uvio::net::websocket {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

class HttpCodec : public Codec<HttpCodec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<HttpRequest>> {
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
        req.method = method;
        req.uri = uri;

        std::string request_headers(2, 0);
        if (auto ret = co_await reader.read_exact(request_headers); !ret) {
            co_return unexpected{ret.error()};
        } else if (request_headers == "\r\n") {
            // no request_headers
            co_return req;
        }

        if (auto ret = co_await reader.read_until(request_headers, "\r\n\r\n");
            !ret) {
            co_return unexpected{ret.error()};
        }
        LOG_DEBUG("{}", request_headers);
        // Host: xxx
        // Content-Type: application/html
        auto headers = parse_headers(request_headers);
        req.headers = headers;

        std::string body;

        if (auto has_length = headers.find("Content-Length")) {
            auto length = std::stoi(has_length.value());
            LOG_DEBUG("Parsed body length: {}", length);
            body.resize(length);
            if (auto ret = co_await reader.read_exact(body); !ret) {
                co_return unexpected{ret.error()};
            }
            LOG_DEBUG("body: `{}`", body);
            req.body = body;
        }

        // 根据是否有 "upgrade" 决定接下来如何处理:
        // 有 upgrade: 检查 sec-websocket-version/sec-websocket-key, 发送响应:
        // HTTP/1.1 101 Switching Protocols 没有: 当成普通的http请求,
        // 返回http响应
        // => 放到 encode() 完成后再做
        if (auto has_upgrade = headers.find("Upgrade")) {
            co_return req;
        }

        co_return req;
    }

    template <typename Writer>
    auto encode(const HttpResponse &resp, Writer &writer)
        -> Task<Result<void>> {
        LOG_DEBUG("websocket response encoding ...");
        // if (auto ret = co_await writer.write(
        //         std::format("HTTP/1.0 200 OK\r\nContent-Length:
        //         {}\r\n\r\n{}",
        //                     message.size(),
        //                     std::string_view{message.data(),
        //                     message.size()}));
        //     !ret) {
        //     co_return unexpected{ret.error()};
        // }

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

class WebsocketCodec : public Codec<WebsocketCodec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<std::string>> {
        co_return std::string{"hello"};
    }

    template <typename Writer>
    auto encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        // writer.write(message);
        co_return Result<void>{};
    }
};

class WebsocketFramed {
public:
    explicit WebsocketFramed(TcpStream &&stream) {
        auto [reader, writer] = std::move(stream).into_split();
        buffered_reader_ = BufferedReader{std::move(reader), 1024};
        buffered_writer_ = BufferedWriter{std::move(writer), 1024};
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto write_response(HttpResponse resp) -> Task<Result<void>> {
        co_return co_await http_codec_.Encode<void>(resp, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<HttpRequest>> {
        co_return co_await http_codec_.Decode<HttpRequest>(buffered_reader_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto recv() -> Task<Result<std::string>> {
        co_return co_await websocket_codec_.Decode<std::string>(
            buffered_reader_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto send(std::span<const char> message) -> Task<Result<void>> {
        co_return co_await websocket_codec_.Encode<void>(message,
                                                         buffered_writer_);
    }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr},
                                    1024};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr},
                                    1024};
    HttpCodec      http_codec_{};
    WebsocketCodec websocket_codec_{};
};
} // namespace uvio::net::websocket
