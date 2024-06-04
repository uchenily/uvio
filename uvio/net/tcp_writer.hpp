#pragma once
#include "uvio/io/split.hpp"
#include "uvio/io/writer.hpp"
#include "uvio/net/tcp_stream.hpp"

namespace uvio::net {

using TcpWriter = io::BufWriter<io::OwnedWriteHalf<TcpStream>>;

} // namespace uvio::net
