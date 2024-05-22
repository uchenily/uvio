#pragma once

#include "uvio/codec.hpp"
#include "uvio/core.hpp"
#include "uvio/net.hpp"

namespace example {

using namespace uvio;
using namespace uvio::net;
using namespace uvio::codec;

using BufferedReader = TcpReader<1024>;
using BufferedWriter = TcpWriter<1024>;

class Channel {
public:
    explicit Channel(TcpStream &&stream) {
        auto [reader, writer] = stream.into_split();
        buffered_reader_ = std::move(reader);
        buffered_writer_ = std::move(writer);
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto Send(std::span<const char> message) -> Task<void> {
        co_await codec_.Encode(message, buffered_writer_);
    }

    [[REMEMBER_CO_AWAIT]]
    auto Recv() -> Task<std::string> {
        co_return co_await codec_.Decode(buffered_reader_);
    }

    // [[REMEMBER_CO_AWAIT]]
    // auto Close() -> Task<void> {
    //     co_await buffered_reader_.inner()
    //         .reunite(buffered_writer_.inner())
    //         .value()
    //         .close();
    // }

private:
    BufferedReader buffered_reader_{io::OwnedReadHalf<TcpStream>{nullptr}};
    BufferedWriter buffered_writer_{io::OwnedWriteHalf<TcpStream>{nullptr}};
    LengthDelimitedCodec<BufferedReader, BufferedWriter> codec_{};
};

} // namespace example
