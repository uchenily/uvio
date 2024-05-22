#pragma once
#include "uvio/io/split.hpp"
#include "uvio/io/writer.hpp"
#include "uvio/net/tcp_stream.hpp"

namespace uvio::net {

template <int WBUF_SIZE>
using TcpWriter = io::BufWriter<io::OwnedWriteHalf<TcpStream>, WBUF_SIZE>;

} // namespace uvio::net
