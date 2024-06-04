#pragma once

#include "uvio/io/reader.hpp"
#include "uvio/io/split.hpp"
#include "uvio/net/tcp_stream.hpp"

namespace uvio::net {

using TcpReader = io::BufReader<io::OwnedReadHalf<TcpStream>>;

} // namespace uvio::net
