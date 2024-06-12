// client:
// (local resolve)
// curl -v http://www.baidu.com/ --proxy socks5://127.0.0.1:1080
// (remote resolve)
// curl -v http://www.baidu.com/ --proxy socks5h://127.0.0.1:1080

#include "uvio/core.hpp"
#include "uvio/net.hpp"
#include "uvio/net/endian.hpp"

#include <array>
#include <sstream>

#define PORT 1080
#define BUFSIZE (static_cast<size_t>(16 * 1024))

namespace socks5 {

using namespace uvio;
using namespace uvio::net;

// with buf
using BufferedReader = TcpReader;
using BufferedWriter = TcpWriter;
// no buf
using Reader = io::OwnedReadHalf<TcpStream>;
using Writer = io::OwnedWriteHalf<TcpStream>;

// class Socks5Codec : public Codec<Socks5Codec> {};

// https://datatracker.ietf.org/doc/html/rfc1928
class Socks5Framed {
public:
    // CMD
    // o  CONNECT X'01'
    // o  BIND X'02'
    // o  UDP ASSOCIATE X'03'
    enum class Cmd {
        CONNECT,
        BIND,
        UDP_ASSOCIATE,
    };

    struct Request {
        Cmd         cmd{};
        std::string addr;
        int         port{};
    };

public:
    explicit Socks5Framed(TcpStream &&stream) {
        auto [reader, writer] = std::move(stream).into_split();
        buffered_reader_ = BufferedReader{std::move(reader), BUFSIZE};
        buffered_writer_ = BufferedWriter{std::move(writer), BUFSIZE};
    }

public:
    [[REMEMBER_CO_AWAIT]]
    auto auth_methods() -> Task<Result<void>> {
        // +----+----------+----------+
        // |VER | NMETHODS | METHODS  |
        // +----+----------+----------+
        // | 1  |    1     | 1 to 255 |
        // +----+----------+----------+
        std::array<char, 2> bytes2{};
        co_await buffered_reader_.read_exact(bytes2);

        if (bytes2[0] != 0x05) {
            co_return uvio::unexpected{make_uvio_error(Error::Unclassified)};
        }
        auto              n_methods = bytes2[1];
        std::vector<char> methods(n_methods);
        co_await buffered_reader_.read_exact(methods);

        // +----+--------+
        // |VER | METHOD |
        // +----+--------+
        // | 1  |   1    |
        // +----+--------+
        // method selection message
        bytes2[0] = 0x05;
        // o  X'00' NO AUTHENTICATION REQUIRED
        // o  X'01' GSSAPI
        // o  X'02' USERNAME/PASSWORD
        // o  X'03' to X'7F' IANA ASSIGNED
        // o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
        // o  X'FF' NO ACCEPTABLE METHODS
        bytes2[1] = 0x00;
        co_await buffered_writer_.write(bytes2);
        co_await buffered_writer_.flush();

        co_return Result<void>{};
    }

    [[REMEMBER_CO_AWAIT]]
    auto read_request() -> Task<Result<Request>> {
        Request request;
        // +----+-----+-------+------+----------+----------+
        // |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        // +----+-----+-------+------+----------+----------+
        // | 1  |  1  | X'00' |  1   | Variable |    2     |
        // +----+-----+-------+------+----------+----------+
        // Where:
        //
        //      o  VER    protocol version: X'05'
        //      o  CMD
        //         o  CONNECT X'01'
        //         o  BIND X'02'
        //         o  UDP ASSOCIATE X'03'
        //      o  RSV    RESERVED
        //      o  ATYP   address type of following address
        //         o  IP V4 address: X'01'
        //         o  DOMAINNAME: X'03'
        //         o  IP V6 address: X'04'
        //      o  DST.ADDR       desired destination address
        //      o  DST.PORT desired destination port in network octet
        //         order
        std::array<char, 2> bytes2{};
        std::array<char, 4> bytes4{};
        co_await buffered_reader_.read_exact(bytes4);
        if (bytes4[0] != 0x05) {
            co_return uvio::unexpected{make_uvio_error(Error::Unclassified)};
        }

        if (bytes4[1] == 0x01) {
            // connect
            request.cmd = Cmd::CONNECT;
        } else if (bytes4[1] == 0x02) {
            // bind
            request.cmd = Cmd::BIND;
        } else if (bytes4[1] == 0x03) {
            // udp associate
            request.cmd = Cmd::UDP_ASSOCIATE;
        } else {
            co_return uvio::unexpected{make_uvio_error(Error::Unclassified)};
        }

        ASSERT(bytes4[2] == 0x00);

        if (bytes4[3] == 0x01) {
            // ipv4
            co_await buffered_reader_.read_exact(bytes4);
            request.addr = ip_to_string(bytes4);

            co_await buffered_reader_.read_exact(bytes2);
            uint16_t port = bytes2[0] | bytes2[1] << 8;
            request.port = net::byteorder::ntoh16(port);
        } else if (bytes4[3] == 0x03) {
            // domain name
            std::array<char, 1> byte{};
            co_await buffered_reader_.read_exact(byte);
            std::vector<char> buf(byte[0]);
            co_await buffered_reader_.read_exact(buf);
            request.addr = std::string{buf.data(), buf.size()};
            // LOG_DEBUG("request.addr: {}", request.addr);

            co_await buffered_reader_.read_exact(bytes2);
            uint16_t port = bytes2[0] | bytes2[1] << 8;
            request.port = net::byteorder::ntoh16(port);
            // LOG_DEBUG("request.port: {}", request.port);
        } else if (bytes4[3] == 0x04) {
            // ipv6
            std::vector<char> buf(16);
            co_await buffered_reader_.read_exact(buf);
            // TODO(x)
            // request.addr =

            co_await buffered_reader_.read_exact(bytes2);
            uint16_t port = bytes2[0] | bytes2[1] << 8;
            request.port = net::byteorder::ntoh16(port);
        } else {
            co_return uvio::unexpected{make_uvio_error(Error::Unclassified)};
        }

        co_return request;
    }

    [[REMEMBER_CO_AWAIT]]
    auto write_reply() -> Task<Result<void>> {
        // +----+-----+-------+------+----------+----------+
        // |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
        // +----+-----+-------+------+----------+----------+
        // | 1  |  1  | X'00' |  1   | Variable |    2     |
        // +----+-----+-------+------+----------+----------+
        // Where:
        //
        //      o  VER    protocol version: X'05'
        //      o  REP    Reply field:
        //         o  X'00' succeeded
        //         o  X'01' general SOCKS server failure
        //         o  X'02' connection not allowed by ruleset
        //         o  X'03' Network unreachable
        //         o  X'04' Host unreachable
        //         o  X'05' Connection refused
        //         o  X'06' TTL expired
        //         o  X'07' Command not supported
        //         o  X'08' Address type not supported
        //         o  X'09' to X'FF' unassigned
        //      o  RSV    RESERVED
        //      o  ATYP   address type of following address
        //         o  IP V4 address: X'01'
        //         o  DOMAINNAME: X'03'
        //         o  IP V6 address: X'04'
        //      o  BND.ADDR       server bound address
        //      o  BND.PORT       server bound port in network octet order
        // Fields marked RESERVED (RSV) must be set to X'00'.

        std::array<char, 2> bytes2{};
        std::array<char, 4> bytes4{};
        // version
        bytes4[0] = 0x05;
        // reply
        bytes4[1] = 0x00;
        // reserved
        bytes4[2] = 0x00;
        // address type
        bytes4[3] = 0x01;
        co_await buffered_writer_.write(bytes4);

        // 这个地址似乎并没有什么用, 客户端早就已经知道代理服务地址了
        bytes4[0] = 127;
        bytes4[1] = 0;
        bytes4[2] = 0;
        bytes4[3] = 1;
        // host address -> network address

        co_await buffered_writer_.write(bytes4);

        // TODO(x)
        // auto port = net::byteorder::hton16(PORT);
        // 端口不设置测试好像也没问题
        auto port = net::byteorder::hton16(0);
        bytes2[0] = static_cast<char>(port & 0xFF);
        bytes2[1] = static_cast<char>((port >> 8) & 0xFF);
        co_await buffered_writer_.write(bytes2);
        co_await buffered_writer_.flush();

        co_return Result<void>{};
    }

    [[REMEMBER_CO_AWAIT]]
    auto connect(Request req) -> Task<Result<void>> {
        // TODO(x): do resolve if domain name?
        auto ok = co_await DNS::resolve(req.addr, std::to_string(req.port));
        if (!ok) {
            co_return uvio::unexpected{ok.error()};
        }

        auto remote = co_await TcpStream::connect(ok.value(), req.port);
        if (!remote) {
            LOG_ERROR("Connect {}:{} failed", req.addr, req.port);
            co_return uvio::unexpected{remote.error()};
        }

        auto local
            = reunite(buffered_reader_.inner(), buffered_writer_.inner());
        if (!local) {
            co_return uvio::unexpected{local.error()};
        }

        auto [l_reader, l_writer] = local->into_split();
        auto [r_reader, r_writer] = remote->into_split();

        // local -> remote
        // remote -> local
        spawn(forward(std::move(l_reader), std::move(r_writer))); // outbound
        spawn(forward(std::move(r_reader), std::move(l_writer))); // inbound
        co_return Result<void>{};
    }

private:
    auto forward(Reader reader, Writer writer) -> Task<> {
        // std::array<char, 1024> buf{};
        std::vector<char> buf(static_cast<size_t>(BUFSIZE));
        while (true) {
            auto rret = co_await reader.read(buf);
            if (!rret) {
                LOG_ERROR("{}", rret.error().message());
                co_return;
            }
            // LOG_DEBUG("{}", std::string_view{buf.data(), rret.value()});
            // LOG_DEBUG("rret: {}", rret.value());
            auto wret = co_await writer.write({buf.data(), rret.value()});
            if (!wret) {
                LOG_ERROR("{}", wret.error().message());
                co_return;
            }
            ASSERT(rret.value() == wret.value());
        }
    }

    auto ip_to_string(const std::array<char, 4> &ipv4) -> std::string {
        std::stringstream ss;
        ss << static_cast<int>(static_cast<unsigned char>(ipv4[0])) << '.'
           << static_cast<int>(static_cast<unsigned char>(ipv4[1])) << '.'
           << static_cast<int>(static_cast<unsigned char>(ipv4[2])) << '.'
           << static_cast<int>(static_cast<unsigned char>(ipv4[3]));
        return ss.str();
    }

private:
    BufferedReader buffered_reader_;
    BufferedWriter buffered_writer_;
};

class Socks5Server {
public:
    Socks5Server(std::string_view host, int port)
        : host_{host}
        , port_{port} {}

public:
    auto run() {
        block_on([this]() -> Task<> {
            auto listener = TcpListener();
            listener.bind(this->host_, this->port_);
            console.info("Listening on {}:{} ...", this->host_, this->port_);
            while (true) {
                auto stream = (co_await listener.accept()).value();
                spawn(this->handle_proxy(std::move(stream)));
            }
        }());
    }

private:
    auto handle_proxy(TcpStream stream) -> Task<> {
        Socks5Framed socks5_framed{std::move(stream)};

        if (auto ok = co_await socks5_framed.auth_methods(); !ok) {
            LOG_ERROR("{}", ok.error().message());
            co_return;
        }

        auto has_req = co_await socks5_framed.read_request();
        if (!has_req) {
            LOG_ERROR("{}", has_req.error().message());
            co_return;
        }
        auto req = std::move(has_req.value());

        if (auto ok = co_await socks5_framed.write_reply(); !ok) {
            LOG_ERROR("{}", ok.error().message());
            co_return;
        }

        switch (req.cmd) {
        case Socks5Framed::Cmd::CONNECT:
            co_await socks5_framed.connect(req);
            break;
        case Socks5Framed::Cmd::BIND:
            // TODO(x)
        case Socks5Framed::Cmd::UDP_ASSOCIATE:
            // TODO(x)
            break;
        }
    }

private:
    std::string host_;
    int         port_;
};
} // namespace socks5

auto main() -> int {
    socks5::Socks5Server proxy{"0.0.0.0", PORT};
    proxy.run();
}
