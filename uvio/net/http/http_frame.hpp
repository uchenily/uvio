#pragma once

#include "uvio/codec/http.hpp"
#include "uvio/common/expected.hpp"
#include "uvio/common/result.hpp"
#include "uvio/core.hpp"
#include "uvio/debug.hpp"
#include "uvio/net/tcp_reader.hpp"
#include "uvio/net/tcp_writer.hpp"

namespace uvio::net::http {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

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
        co_return co_await codec_.Encode<void>(resp, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<HttpRequest>> {
        co_return co_await codec_.Decode<HttpRequest>(buffered_reader_);
    }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr},
                                    1024};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr},
                                    1024};
    HttpCodec      codec_{};
};
} // namespace uvio::net::http
