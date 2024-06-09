#pragma once

#include "uvio/codec/http.hpp"
#include "uvio/codec/websocket.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/http/http_protocol.hpp"
#include "uvio/net/tcp_reader.hpp"
#include "uvio/net/tcp_writer.hpp"

namespace uvio::net::websocket {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

class WebsocketFramed {
public:
    explicit WebsocketFramed(TcpStream &&stream) {
        auto [reader, writer] = std::move(stream).into_split();
        buffered_reader_ = BufferedReader{std::move(reader), 1024};
        buffered_writer_ = BufferedWriter{std::move(writer), 1024};
    }

public:
    // server
    [[REMEMBER_CO_AWAIT]]
    auto send_response(const http::HttpResponse &resp) -> Task<Result<void>> {
        co_return co_await http_codec_.Encode<void>(resp, buffered_writer_);
    }

    // server
    [[REMEMBER_CO_AWAIT]]
    auto recv_request() -> Task<Result<http::HttpRequest>> {
        http::HttpRequest req;
        if (auto res = co_await http_codec_.Decode<void>(req, buffered_reader_);
            !res) {
            co_return unexpected{res.error()};
        }
        co_return std::move(req);
    }

    // client
    [[REMEMBER_CO_AWAIT]]
    auto send_request(const http::HttpRequest &req) -> Task<Result<void>> {
        co_return co_await http_codec_.Encode<void>(req, buffered_writer_);
    }

    // client
    [[REMEMBER_CO_AWAIT]]
    auto recv_response() -> Task<Result<http::HttpResponse>> {
        http::HttpResponse resp;
        if (auto res
            = co_await http_codec_.Decode<void>(resp, buffered_reader_);
            !res) {
            co_return unexpected{res.error()};
        }
        co_return std::move(resp);
    }

    [[REMEMBER_CO_AWAIT]]
    auto recv() -> Task<Result<std::vector<char>>> {
        std::vector<char> payload;
        if (auto res
            = co_await websocket_codec_.Decode<void>(payload, buffered_reader_);
            !res) {
            co_return unexpected{res.error()};
        }
        co_return std::move(payload);
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
