#pragma once

#include "uvio/codec.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/http/http_protocol.hpp"
#include "uvio/net/tcp_reader.hpp"
#include "uvio/net/tcp_writer.hpp"
#include "uvio/net/websocket/websocket_protocol.hpp"

namespace uvio::net::websocket {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

class HttpCodec : public Codec<HttpCodec> {
public:
    template <typename Reader>
    auto decode(Reader &reader) -> Task<Result<http::HttpRequest>> {
        http::HttpRequest req;
        std::string       request_line;
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

        if (auto has_length = headers.find("Content-Length")) {
            std::string body;
            auto        length = std::stoi(has_length.value());
            LOG_DEBUG("Parsed body length: {}", length);
            body.resize(length);
            if (auto ret = co_await reader.read_exact(body); !ret) {
                co_return unexpected{ret.error()};
            }
            LOG_DEBUG("body: `{}`", body);
            req.body = std::move(body);
        }

        co_return req;
    }

    template <typename Writer>
    auto encode(http::HttpResponse &resp, Writer &writer)
        -> Task<Result<void>> {
        if (!resp.headers.find("Upgrade")) {
            // 普通的 HTTP 请求
            if (auto ret = co_await writer.write(std::format(
                    "HTTP/1.0 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                    resp.body.size(),
                    resp.body));
                !ret) {
                co_return unexpected{ret.error()};
            }
            if (auto ret = co_await writer.flush(); !ret) {
                co_return ret;
            }
            co_return Result<void>{};
        }

        // Websocket 握手
        //
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
        std::array<char, 2>          buf;
        std::array<char, 4>          mask_bits{};
        std::array<unsigned char, 8> buf8{};
        co_await reader.read_exact(buf);
        bool fin = buf[0] & 0b100000000;
        auto opcode = buf[0] & 0b00001111;
        ASSERT(fin);
        ASSERT(opcode == (int) Opcode::TEXT);

        // auto length = static_cast<size_t>(buf[1]); // error
        uint64_t length = (static_cast<unsigned char>(buf[1]) & 0b01111111);
        LOG_DEBUG("pre: {}", length);
        if (length == 126) {
            co_await reader.read_exact(buf);
            length = buf[0] << 8 | buf[1];
        } else if (length == 127) {
            co_await reader.read_exact(
                {reinterpret_cast<char *>(buf8.data()), buf8.size()});
            length = static_cast<uint64_t>(buf[0])
                     | static_cast<uint64_t>(buf8[1]) << 8
                     | static_cast<uint64_t>(buf8[2]) << 16
                     | static_cast<uint64_t>(buf8[3]) << 24
                     | static_cast<uint64_t>(buf8[4]) << 32
                     | static_cast<uint64_t>(buf8[5]) << 40
                     | static_cast<uint64_t>(buf8[6]) << 48
                     | static_cast<uint64_t>(buf8[7]) << 56;
        }
        LOG_DEBUG("payload length: {}", length);

        if (!client_side_) {
            co_await reader.read_exact(mask_bits);
        }

        auto payload = std::vector<char>(length);
        co_await reader.read_exact(payload);
        if (!client_side_) {
            apply_mask(payload, mask_bits);
        }
        co_return payload;
    }

    template <typename Writer>
    auto encode(WebsocketFrame frame, Writer &writer) -> Task<Result<void>> {
        // https://github.com/python-websockets/websockets/blob/12.0/src/websockets/frames.py#L273
        std::array<unsigned char, 10> buf{};
        buf[0] = static_cast<char>(0b1000'0000)
                 | static_cast<char>(frame.opcode); // fin + opcode
        buf[1] = client_side_ ? static_cast<char>(0b1000'0000) : 0; // mask
        auto length = frame.payload_length();
        if (length < 126) {
            buf[1] |= length;
            co_await writer.write(
                std::span<char, 2>{reinterpret_cast<char *>(buf.data()), 2});
        } else if (length < 65536) {
            buf[1] |= 126;
            buf[2] = length | 0xFF;
            buf[3] = (length >> 8) | 0xFF;
            buf[4] = (length >> 16) | 0xFF;
            buf[5] = (length >> 24) | 0xFF;
            co_await writer.write(
                std::span<char, 6>{reinterpret_cast<char *>(buf.data()), 6});
        } else {
            buf[1] |= 127;
            buf[2] = length | 0xFF;
            buf[3] = (length >> 8) | 0xFF;
            buf[4] = (length >> 16) | 0xFF;
            buf[5] = (length >> 24) | 0xFF;
            buf[6] = (length >> 32) | 0xFF;
            buf[7] = (length >> 40) | 0xFF;
            buf[8] = (length >> 48) | 0xFF;
            buf[9] = (length >> 56) | 0xFF;
            co_await writer.write(
                {reinterpret_cast<char *>(buf.data()), buf.size()});
        }

        if (client_side_) {
            // TODO(x)
            auto mask = std::array<char, 4>{1, 1, 1, 1};
            co_await writer.write(mask);
            apply_mask(frame.message, mask);
        }

        co_await writer.write(frame.message);
        co_await writer.flush();

        co_return Result<void>{};
    }

    auto client_side() {
        client_side_ = true;
    }

    auto apply_mask(std::span<char> data, const std::array<char, 4> &mask)
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
    bool client_side_{false};
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
    auto write_response(http::HttpResponse resp) -> Task<Result<void>> {
        co_return co_await http_codec_.Encode<void>(resp, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<http::HttpRequest>> {
        co_return co_await http_codec_.Decode<http::HttpRequest>(
            buffered_reader_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto recv() -> Task<Result<std::vector<char>>> {
        co_return co_await websocket_codec_.Decode<std::vector<char>>(
            buffered_reader_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto send(std::span<char> message) -> Task<Result<void>> {
        WebsocketFrame frame{
            .opcode = Opcode::TEXT,
            .message = message,
        };
        co_return co_await websocket_codec_.Encode<void>(frame,
                                                         buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto close() -> Task<Result<void>> {
        WebsocketFrame frame{
            .opcode = Opcode::CLOSE,
            .message = {},
        };
        co_return co_await websocket_codec_.Encode<void>(frame,
                                                         buffered_writer_);
    }

    auto client_side() {
        websocket_codec_.client_side();
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
