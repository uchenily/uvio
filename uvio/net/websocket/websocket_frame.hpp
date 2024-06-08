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
    auto decode(Reader &reader) -> Task<Result<std::vector<char>>> {
        // https://github.com/python-websockets/websockets/blob/12.0/src/websockets/legacy/framing.py
        std::array<char, 2> buf;
        std::array<char, 4> mask_bits{};
        std::array<char, 8> buf8{};
        co_await reader.read_exact(buf);
        bool fin = buf[0] & 0b100000000;
        auto opcode = buf[0] & 0b00001111;
        // auto length = static_cast<size_t>(buf[1]);
        uint64_t length = (static_cast<unsigned char>(buf[1]) & 0b01111111);
        LOG_DEBUG("pre: {}", length);
        if (length == 126) {
            co_await reader.read_exact(buf);
            length = buf[0] << 8 | buf[1];
        } else if (length == 127) {
            co_await reader.read_exact(buf8);
            length = buf8[7] << 56 | buf8[6] << 48 | buf8[5] << 40
                     | buf8[4] << 32 | buf8[3] << 24 | buf8[2] << 16
                     | buf8[1] << 8 | buf[0];
        }
        LOG_DEBUG("payload length: {}", length);

        if (mask_) {
            co_await reader.read_exact(mask_bits);
        }

        auto payload = std::vector<char>(length);
        co_await reader.read_exact(payload);
        if (mask_) {
            apply_mask(payload, mask_bits);
        }
        LOG_DEBUG("payload: {}",
                  std::string_view{payload.data(), payload.size()});

        co_return payload;
    }

    template <typename Writer>
    auto encode(std::span<const char> message, Writer &writer)
        -> Task<Result<void>> {
        // writer.write(message);
        co_return Result<void>{};
    }

    auto enable_mask() {
        mask_ = true;
    }

    auto apply_mask(std::vector<char> &data, const std::array<char, 4> &mask)
        -> void {
        auto length = data.size();
        for (auto i = 0u; i < (length & ~3); i += 4) {
            data[i + 0] ^= mask[0];
            data[i + 1] ^= mask[1];
            data[i + 2] ^= mask[2];
            data[i + 3] ^= mask[3];
        }

        for (auto i = length & ~3; i < data.size(); ++i) {
            data[i] ^= mask[i % 4];
        }
    }

private:
    bool mask_{false};
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
    auto recv() -> Task<Result<std::vector<char>>> {
        co_return co_await websocket_codec_.Decode<std::vector<char>>(
            buffered_reader_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto send(std::span<const char> message) -> Task<Result<void>> {
        co_return co_await websocket_codec_.Encode<void>(message,
                                                         buffered_writer_);
    }

    auto server_side() {
        websocket_codec_.enable_mask();
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
