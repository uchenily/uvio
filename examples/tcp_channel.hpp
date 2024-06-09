#pragma once

#include "uvio/codec/fixed_length.hpp"
#include "uvio/codec/length_delimited.hpp"
#include "uvio/core.hpp"
#include "uvio/net.hpp"

namespace example {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;

class Channel {
public:
    explicit Channel(TcpStream &&stream) {
        auto [reader, writer] = std::move(stream).into_split();
        buffered_reader_ = BufferedReader{std::move(reader), 1024};
        buffered_writer_ = BufferedWriter{std::move(writer), 1024};
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto Send(std::span<const char> message) -> Task<Result<void>> {
        co_return co_await codec_.Encode<void>(message, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto Recv() -> Task<Result<std::string>> {
        std::string message;
        if (auto res = co_await codec_.Decode<void>(message, buffered_reader_);
            !res) {
            co_return unexpected{res.error()};
        }
        co_return std::move(message);
    }

    // [[REMEMBER_CO_AWAIT]]
    // auto Close() -> Task<void> {
    //     co_await buffered_reader_.inner()
    //         .reunite(buffered_writer_.inner())
    //         .value()
    //         .close();
    // }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr},
                                    1024};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr},
                                    1024};
    LengthDelimitedCodec codec_{};
    // FixedLength32Codec codec_;
};

} // namespace example
