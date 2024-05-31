#pragma once

#include "uvio/codec.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net.hpp"
#include "uvio/net/http/http_protocol.hpp"

namespace uvio::net::http {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader<1024>;
using BufferedWriter = TcpWriter<1024>;

template <typename Reader, typename Writer>
class HttpCodec : public Codec<HttpCodec<Reader, Writer>, Reader, Writer> {
public:
    auto decode(Reader &reader) -> Task<Result<std::string>> {
        std::string message;
        if (auto ret = co_await reader.read_until(message, "\r\n"); !ret) {
            co_return unexpected{ret.error()};
        }
        LOG_DEBUG("{}", message);

        std::string message2;
        if (auto ret = co_await reader.read_until(message2, "\r\n\r\n"); !ret) {
            co_return unexpected{ret.error()};
        }
        LOG_DEBUG("{}", message2);

        // std::string message3;
        // if (auto ret = co_await reader.read(message3); !ret) {
        //     co_return unexpected{ret.error()};
        // }
        // LOG_DEBUG("{}", message3);

        co_return "fake";
    }

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
};

class HttpFramed {
public:
    explicit HttpFramed(TcpStream &&stream) {
        auto [reader, writer] = stream.into_split();
        buffered_reader_ = std::move(reader);
        buffered_writer_ = std::move(writer);
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto write_response(HttpResponse resp) -> Task<Result<void>> {
        co_return co_await codec_.Encode(resp.body, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<HttpRequest>> {
        auto result = co_await codec_.Decode(buffered_reader_);
        co_return HttpRequest{.body = result.value()};
    }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr}};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr}};
    HttpCodec<BufferedReader, BufferedWriter> codec_{};
};
} // namespace uvio::net::http
