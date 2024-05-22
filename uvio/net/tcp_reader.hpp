#pragma once

#include "uvio/io/reader.hpp"
#include "uvio/io/split.hpp"
#include "uvio/net/tcp_stream.hpp"

namespace uvio::net {

template <int RBUF_SIZE>
using TcpReader = io::BufReader<io::OwnedReadHalf<TcpStream>, RBUF_SIZE>;

} // namespace uvio::net
