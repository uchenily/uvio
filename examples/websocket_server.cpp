#include "uvio/debug.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <uv.h>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "websocket_utils.hpp"

class Connection;
class Server;
namespace detail {
struct Corker;
struct SocketDeleter {
    void operator()(uv_tcp_t *socket) const {
        if (socket == nullptr) {
            return;
        }
        uv_close(reinterpret_cast<uv_handle_t *>(socket), [](uv_handle_t *h) {
            delete reinterpret_cast<uv_tcp_t *>(h);
        });
    }
};
} // namespace detail

using TcpHandle = std::unique_ptr<uv_tcp_t, detail::SocketDeleter>;

class RequestHeaders {
public:
    void Set(std::string_view key, std::string_view value) {
        m_Headers.emplace_back(key, value);
    }

    template <typename F>
    void ForEachValueOf(std::string_view key, const F &f) const {
        for (auto &p : m_Headers) {
            if (p.first == key) {
                f(p.second);
            }
        }
    }

    auto Get(std::string_view key) const -> std::optional<std::string_view> {
        for (auto &p : m_Headers) {
            if (p.first == key) {
                return p.second;
            }
        }

        return std::nullopt;
    }

    template <typename F>
    void ForEach(const F &f) const {
        for (auto &p : m_Headers) {
            f(p.first, p.second);
        }
    }

private:
    std::vector<std::pair<std::string_view, std::string_view>> m_Headers;

    friend class Connection;
    friend class Server;
};

struct HTTPRequest {
    Server          *server;
    std::string_view method;
    std::string_view path;
    std::string_view ip;

    // Header keys are always lower case
    const RequestHeaders &headers;
};

class HTTPResponse {
public:
    auto status(int v) -> HTTPResponse & {
        statusCode = v;
        return *this;
    }
    auto send(std::string_view v) -> HTTPResponse & {
        body.append(v);
        return *this;
    }

    // Appends a response header. The following headers cannot be changed:
    // Connection: close
    // Content-Length: body.size()
    auto header(std::string_view key, std::string_view value)
        -> HTTPResponse & {
        headers.emplace(std::string(key), std::string(value));
        return *this;
    }

private:
    int                                     statusCode = 200;
    std::string                             body;
    std::multimap<std::string, std::string> headers;

    friend class Connection;
};

class Connection {
    enum { MAX_HEADER_SIZE = 10 };
    enum : unsigned char { NO_FRAMES = 0 };

public:
    ~Connection();

    // If reasonLen is -1, it'll use strlen
    void
    Close(uint16_t code, const char *reason = nullptr, size_t reasonLen = -1);
    void Destroy();
    void Send(const char *data, size_t len, uint8_t opCode = 2);

    inline void SetUserData(void *v) {
        m_pUserData = v;
    }
    inline auto GetUserData() -> void * {
        return m_pUserData;
    }

    inline auto IsUsingAlternativeProtocol() const -> bool {
        return m_bUsingAlternativeProtocol;
    }

    inline auto GetServer() -> Server * {
        return m_pServer;
    }

    inline auto GetIP() const -> const char * {
        return m_IP;
    }

    inline auto HasConnectionRequestedClose() const -> bool {
        return m_bConnectionRequestedClose;
    }

private:
    struct DataFrame {
        uint8_t                 opcode;
        std::unique_ptr<char[]> data;
        size_t                  len;
    };

    Connection(Server *server, TcpHandle socket);

    Connection(const Connection &other) = delete;
    auto operator=(Connection &other) -> Connection & = delete;

    auto GetDataFrameHeaderSize(size_t len) -> size_t;
    void WriteDataFrameHeader(uint8_t opcode, size_t len, char *headerStart);
    void EncryptAndWrite(const char *data, size_t len);

    void OnRawSocketData(char *data, size_t len);
    void OnSocketData(char *data, size_t len);
    void ProcessDataFrame(uint8_t opcode, char *data, size_t len);

    void Write(const char *data);
    void Write(const char *data, size_t len);

    template <size_t N>
    void Write(uv_buf_t bufs[N]);

    template <size_t N>
    void WriteRaw(uv_buf_t bufs[N]);

    void WriteRawQueue(std::unique_ptr<char[]> data, size_t len);

    void Cork(bool v);

    // Stub, maybe some day
    inline auto IsValidUTF8(const char * /*unused*/, size_t /*unused*/)
        -> bool {
        return true;
    }

    inline auto IsBuildingFrames() const -> bool {
        return m_iFrameOpcode != NO_FRAMES;
    }

    Server   *m_pServer;
    TcpHandle m_Socket;
    void     *m_pUserData = nullptr;
    bool      m_bWaitingForFirstPacket = true;
    bool      m_bHasCompletedHandshake = false;
    bool      m_bIsClosing = false;
    bool      m_bUsingAlternativeProtocol = false;
    bool      m_bConnectionRequestedClose = false;
    char      m_IP[46];

    std::vector<char> m_Buffer;
    uint8_t           m_iFrameOpcode = NO_FRAMES;
    std::vector<char> m_FrameBuffer;

    friend class Server;
    friend struct detail::Corker;
    friend class std::unique_ptr<Connection>;
};

class Server {
public:
    using CheckConnectionFn = bool (*)(HTTPRequest &);
    using CheckAlternativeConnectionFn = bool (*)(Connection *);
    using ConnectionConnectedFn = void (*)(HTTPRequest &);
    using ConnectionDisconnectedFn = void (*)();
    using ConnectionDataFn = void (*)(Connection *, char *, size_t, int);
    using HTTPRequestFn = void (*)(HTTPRequest &, HTTPResponse &);

public:
    // Note: By default, this listens on both ipv4 and ipv6
    // Note: if you provide a SSL_CTX, this server will listen to *BOTH* secure
    // and insecure connections at that port,
    //       sniffing the first byte to figure out whether it's secure or not
    Server(uv_loop_t *loop)
        : loop_(loop) {

        CheckConnection_cb = [](HTTPRequest &req) -> bool {
            auto host = req.headers.Get("host");
            if (!host) {
                return true; // No host header, default to accept
            }

            auto origin = req.headers.Get("origin");
            if (!origin) {
                return true;
            }

            return origin == host;
        };
    }

    Server(const Server &other) = delete;
    auto operator=(const Server &other) -> Server & = delete;
    ~Server() {
        StopListening();
        DestroyConnections();
    }

    auto Listen(int port, bool ipv4Only = false) -> bool {
        if (handle_) {
            return false;
        }

        signal(SIGPIPE, SIG_IGN);

        auto server = TcpHandle{new uv_tcp_t};
        uv_tcp_init_ex(loop_, server.get(), ipv4Only ? AF_INET : AF_INET6);
        server->data = this;

        struct sockaddr_storage addr;

        if (ipv4Only) {
            uv_ip4_addr("0.0.0.0",
                        port,
                        reinterpret_cast<struct sockaddr_in *>(&addr));
        } else {
            uv_ip6_addr("::0",
                        port,
                        reinterpret_cast<struct sockaddr_in6 *>(&addr));
        }

        uv_tcp_nodelay(server.get(), static_cast<int>(true));

        // Enable SO_REUSEPORT
        uv_os_fd_t fd = 0;
        int const  r
            = uv_fileno(reinterpret_cast<uv_handle_t *>(server.get()), &fd);
        (void) r;
        assert(r == 0);
        int optval = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (uv_tcp_bind(server.get(),
                        reinterpret_cast<struct sockaddr *>(&addr),
                        0)
            != 0) {
            return false;
        }

        if (uv_listen(reinterpret_cast<uv_stream_t *>(server.get()),
                      512,
                      [](uv_stream_t *server, int status) {
                          (static_cast<Server *>(server->data))
                              ->OnConnection(server, status);
                      })
            != 0) {
            return false;
        }

        handle_ = std::move(server);
        return true;
    }

    void StopListening() {
        // Just in case we have more logic in the future
        if (!handle_) {
            return;
        }
        handle_.reset();
    }

    void DestroyConnections() {
        // Connections will erase themselves from this vector
        while (!connections_.empty()) {
            connections_.back()->Destroy();
        }
    }

    // // This callback is called when the client is trying to connect using
    // // websockets By default, for safety, this checks the Origin and makes
    // sure
    // // it matches the Host It's likely you wanna change this check if your
    // // websocket server is in a different domain.
    // void SetCheckConnectionCallback(CheckConnectionFn v) {
    //     CheckConnection_cb = v;
    // }

    // This is called instead of CheckConnection for connections using the
    // alternative protocol (if enabled)
    void SetCheckAlternativeConnectionCallback(CheckAlternativeConnectionFn v) {
        CheckAlternativeConnection_cb = v;
    }

    // This callback is called when a client establishes a connection (after
    // websocket handshake) This is paired with the disconnected callback
    void SetConnectionConnectedCallback(ConnectionConnectedFn v) {
        ConnectionConnected_cb = v;
    }

    // This callback is called when a client disconnects
    // This is paired with the connected callback, and will *always* be called
    // for clients that called the other callback Note that clients grab this
    // value when you call Destroy on them, so changing this after clients are
    // connected might lead to weird results. In practice, just set it once and
    // forget about it.
    void SetConnectionDisconnectedCallback(ConnectionDisconnectedFn v) {
        ConnectionDisconnected_cb = v;
    }

    // This callback is called when the client receives a data frame
    // Note that both text and binary op codes end up here
    void SetConnectionDataCallback(ConnectionDataFn v) {
        ConnectionData_cb = v;
    }

    // This callback is called when a normal http request is received
    // If you don't send anything in response, the status code is 404
    // If you send anything in response without setting a specific status code,
    // it will be 200 Connections that call this callback never lead to a
    // connection
    void SetHTTPCallback(HTTPRequestFn v) {
        HTTPRequest_cb = v;
    }

    inline void SetUserData(void *v) {
        data_ = v;
    }
    inline auto GetUserData() const -> void * {
        return data_;
    }

    // Alternative protocol means that the client sends a 0x00, and we skip all
    // websocket protocol This means clients don't call CheckConnection, and
    // they receive an empty request header in the connection callback Opcode is
    // always binary In the alternative protocol, clients and servers send the
    // packet length as a Little Endian uint32, then its contents
    inline void SetAllowAlternativeProtocol(bool v) {
        allow_alternative_protocol_ = v;
    }
    inline auto GetAllowAlternativeProtocol() const -> bool {
        return allow_alternative_protocol_;
    }

    void Ref() const {
        if (handle_) {
            uv_ref(reinterpret_cast<uv_handle_t *>(handle_.get()));
        }
    }
    void Unref() const {
        if (handle_) {
            uv_unref(reinterpret_cast<uv_handle_t *>(handle_.get()));
        }
    }

    // private:
    // TODO(x)
public:
    void OnConnection(uv_stream_t *server, int status) {
        if (status < 0) {
            return;
        }

        TcpHandle socket{new uv_tcp_t};
        uv_tcp_init(loop_, socket.get());

        socket->data = nullptr;

        if (uv_accept(server, reinterpret_cast<uv_stream_t *>(socket.get()))
            == 0) {
            auto conn = new Connection(this, std::move(socket));
            connections_.emplace_back(conn);

            // If for whatever reason uv_tcp_getpeername failed (happens...
            // somehow?)
            if (conn->GetIP()[0] == '\0') {
                conn->Destroy();
            }
        }
    }

    auto NotifyConnectionPreDestroyed(Connection *conn)
        -> std::unique_ptr<Connection> {
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (it->get() == conn) {
                std::unique_ptr<Connection> r = std::move(*it);
                *it = std::move(connections_.back());
                connections_.pop_back();
                return r;
            }
        }

        assert(false);
        return {};
    }

    uv_loop_t                               *loop_;
    TcpHandle                                handle_;
    void                                    *data_{nullptr};
    std::vector<std::unique_ptr<Connection>> connections_;
    bool                                     allow_alternative_protocol_{false};
    size_t max_message_size_{static_cast<size_t>(16 * 1024 * 1024)};

    CheckConnectionFn            CheckConnection_cb = nullptr;
    CheckAlternativeConnectionFn CheckAlternativeConnection_cb = nullptr;
    ConnectionConnectedFn        ConnectionConnected_cb = nullptr;
    ConnectionDisconnectedFn     ConnectionDisconnected_cb = nullptr;
    ConnectionDataFn             ConnectionData_cb = nullptr;
    HTTPRequestFn                HTTPRequest_cb = nullptr;
};

namespace detail {
auto equalsi(std::string_view a, std::string_view b) -> bool {
    if (a.size() != b.size()) {
        return false;
    }
    for (;;) {
        if (tolower(a.front()) != tolower(b.front())) {
            return false;
        }

        a.remove_prefix(1);
        b.remove_prefix(1);
        if (a.empty()) {
            return true;
        }
    }
}

auto equalsi(std::string_view a, std::string_view b, size_t n) -> bool {
    while ((n--) != 0u) {
        if (a.empty()) {
            return b.empty();
        } else if (b.empty()) {
            return false;
        }

        if (tolower(a.front()) != tolower(b.front())) {
            return false;
        }

        a.remove_prefix(1);
        b.remove_prefix(1);
    }

    return true;
}

auto HeaderContains(std::string_view header, std::string_view substring)
    -> bool {
    bool hasMatch = false;

    while (!header.empty()) {
        if (header.front() == ' ' || header.front() == '\t') {
            header.remove_prefix(1);
            continue;
        }

        if (hasMatch) {
            if (header.front() == ',') {
                return true;
            }
            hasMatch = false;
            header.remove_prefix(1);

            // Skip to comma or end of string
            while (!header.empty() && header.front() != ',') {
                header.remove_prefix(1);
            }
            if (header.empty()) {
                return false;
            }

            // Skip comma
            assert(header.front() == ',');
            header.remove_prefix(1);
        } else {
            if (detail::equalsi(header, substring, substring.size())) {
                // We have a match... if the header ends here, or has a
                // comma
                hasMatch = true;
                header.remove_prefix(substring.size());
            } else {
                header.remove_prefix(1);

                // Skip to comma or end of string
                while (!header.empty() && header.front() != ',') {
                    header.remove_prefix(1);
                }
                if (header.empty()) {
                    return false;
                }

                // Skip comma
                assert(header.front() == ',');
                header.remove_prefix(1);
            }
        }
    }

    return hasMatch;
}

struct Corker {
    Connection &conn_;

    Corker(Connection &conn)
        : conn_(conn) {
        conn.Cork(true);
    }
    ~Corker() {
        conn_.Cork(false);
    }
};
} // namespace detail

struct DataFrameHeader {
    char *data;

    DataFrameHeader(char *data)
        : data(data) {}

    void reset() {
        data[0] = 0;
        data[1] = 0;
    }

    void fin(bool v) {
        data[0] &= ~(1 << 7);
        data[0] |= static_cast<int>(v) << 7;
    }
    void rsv1(bool v) {
        data[0] &= ~(1 << 6);
        data[0] |= static_cast<int>(v) << 6;
    }
    void rsv2(bool v) {
        data[0] &= ~(1 << 5);
        data[0] |= static_cast<int>(v) << 5;
    }
    void rsv3(bool v) {
        data[0] &= ~(1 << 4);
        data[0] |= static_cast<int>(v) << 4;
    }
    void mask(bool v) {
        data[1] &= ~(1 << 7);
        data[1] |= static_cast<int>(v) << 7;
    }
    void opcode(uint8_t v) {
        data[0] &= ~0x0F;
        data[0] |= v & 0x0F;
    }

    void len(uint8_t v) {
        data[1] &= ~0x7F;
        data[1] |= v & 0x7F;
    }

    auto fin() const -> bool {
        return ((data[0] >> 7) & 1) != 0;
    }
    auto rsv1() const -> bool {
        return ((data[0] >> 6) & 1) != 0;
    }
    auto rsv2() const -> bool {
        return ((data[0] >> 5) & 1) != 0;
    }
    auto rsv3() const -> bool {
        return ((data[0] >> 4) & 1) != 0;
    }
    auto mask() const -> bool {
        return ((data[1] >> 7) & 1) != 0;
    }

    auto opcode() const -> uint8_t {
        return data[0] & 0x0F;
    }

    auto len() const -> uint8_t {
        return data[1] & 0x7F;
    }
};

Connection::Connection(Server *server, TcpHandle socket)
    : m_pServer(server)
    , m_Socket(std::move(socket)) {
    m_Socket->data = this;

    // Default to true since that's what most people want
    uv_tcp_nodelay(m_Socket.get(), 1);
    uv_tcp_keepalive(m_Socket.get(), 1, 10000);

    { // Put IP in m_IP
        m_IP[0] = '\0';
        struct sockaddr_storage addr;
        int                     addrLen = sizeof(addr);
        uv_tcp_getpeername(m_Socket.get(),
                           reinterpret_cast<sockaddr *>(&addr),
                           &addrLen);

        if (addr.ss_family == AF_INET) {
            int const r = uv_ip4_name(reinterpret_cast<sockaddr_in *>(&addr),
                                      m_IP,
                                      sizeof(m_IP));
            (void) r;
            assert(r == 0);
        } else if (addr.ss_family == AF_INET6) {
            int const r = uv_ip6_name(reinterpret_cast<sockaddr_in6 *>(&addr),
                                      m_IP,
                                      sizeof(m_IP));
            (void) r;
            assert(r == 0);

            // Remove this prefix if it exists, it means that we actually
            // have a ipv4
            static const char *ipv4Prefix = "::ffff:";
            if (strncmp(m_IP, ipv4Prefix, strlen(ipv4Prefix)) == 0) {
                memmove(m_IP,
                        m_IP + strlen(ipv4Prefix),
                        strlen(m_IP) - strlen(ipv4Prefix) + 1);
            }
        } else {
            // Server::OnConnection will destroy us
        }
    }

    uv_read_start(
        reinterpret_cast<uv_stream_t *>(m_Socket.get()),
        [](uv_handle_t *, size_t suggested_size, uv_buf_t *buf) {
            buf->base = new char[suggested_size];
            buf->len = suggested_size;
        },
        [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            auto conn = static_cast<Connection *>(stream->data);

            if (conn != nullptr) {
                if (nread < 0) {
                    conn->Destroy();
                } else if (nread > 0) {
                    conn->OnRawSocketData(buf->base,
                                          static_cast<size_t>(nread));
                }
            }

            if (buf != nullptr) {
                delete[] buf->base;
            }
        });
}

Connection::~Connection() {
    assert(!m_Socket);
}

void Connection::Destroy() {
    if (!m_Socket) {
        return;
    }

    Cork(false);

    m_Socket->data = nullptr;

    auto myself = m_pServer->NotifyConnectionPreDestroyed(this);

    struct ShutdownRequest : uv_shutdown_t {
        TcpHandle                        handle_;
        std::unique_ptr<Connection>      conn_;
        Server::ConnectionDisconnectedFn disconnected_cb_;
    };

    auto req = new ShutdownRequest();
    req->handle_ = std::move(m_Socket);
    req->conn_ = std::move(myself);
    req->disconnected_cb_ = m_pServer->ConnectionDisconnected_cb;

    m_pServer = nullptr;

    static auto cb = [](uv_shutdown_t *reqq, int) {
        auto req = (ShutdownRequest *) reqq;

        if (req->disconnected_cb_ && req->conn_->m_bHasCompletedHandshake) {
            req->disconnected_cb_();
        }

        delete req;
    };

    if (uv_shutdown(req,
                    reinterpret_cast<uv_stream_t *>(req->handle_.get()),
                    cb)
        != 0) {
        // Shutdown failed, but we have to delay the destruction to the next
        // event loop
        auto timer = new uv_timer_t;
        uv_timer_init(req->handle_->loop, timer);
        timer->data = req;
        uv_timer_start(
            timer,
            [](uv_timer_t *timer) {
                auto req = static_cast<ShutdownRequest *>(timer->data);
                cb(req, 0);
                uv_close(reinterpret_cast<uv_handle_t *>(timer),
                         [](uv_handle_t *h) {
                             delete reinterpret_cast<uv_timer_t *>(h);
                         });
            },
            0,
            0);
    }
}

template <size_t N>
void Connection::WriteRaw(uv_buf_t bufs[N]) {
    if (!m_Socket) {
        return;
    }

    // Try to write without allocating memory first, if that doesn't work,
    // we call WriteRawQueue
    int written = uv_try_write(reinterpret_cast<uv_stream_t *>(m_Socket.get()),
                               bufs,
                               N);
    if (written == UV_EAGAIN) {
        written = 0;
    }

    if (written >= 0) {
        size_t totalLength = 0;

        for (size_t i = 0; i < N; ++i) {
            auto &buf = bufs[i];
            totalLength += buf.len;
        }

        auto skipping = static_cast<size_t>(written);
        if (skipping == totalLength) {
            return; // Complete write
        }

        // Partial write
        // Copy the remainder into a buffer to send to WriteRawQueue

        auto   cpy = std::make_unique<char[]>(totalLength);
        size_t offset = 0;

        for (size_t i = 0; i < N; ++i) {
            auto &buf = bufs[i];
            if (skipping >= buf.len) {
                skipping -= buf.len;
                continue;
            }

            memcpy(cpy.get() + offset, buf.base + skipping, buf.len - skipping);
            offset += buf.len - skipping;
            skipping = 0;
        }

        WriteRawQueue(std::move(cpy), offset);
    } else {
        // Write error
        Destroy();
        return;
    }
}

void Connection::WriteRawQueue(std::unique_ptr<char[]> data, size_t len) {
    if (!m_Socket) {
        return;
    }

    struct CustomWriteRequest {
        uv_write_t              req;
        Connection             *conn;
        std::unique_ptr<char[]> data;
    };

    uv_buf_t buf;
    buf.base = data.get();
    buf.len = len;

    auto request = new CustomWriteRequest();
    request->conn = this;
    request->data = std::move(data);

    if (uv_write(&request->req,
                 reinterpret_cast<uv_stream_t *>(m_Socket.get()),
                 &buf,
                 1,
                 [](uv_write_t *req, int status) {
                     auto request = reinterpret_cast<CustomWriteRequest *>(req);

                     if (status < 0) {
                         request->conn->Destroy();
                     }

                     delete request;
                 })
        != 0) {
        delete request;
        Destroy();
    }
}

template <size_t N>
void Connection::Write(uv_buf_t bufs[N]) {
    if (!m_Socket) {
        return;
    }
    WriteRaw<N>(bufs);
}

void Connection::Write(const char *data, size_t len) {
    uv_buf_t bufs[1];
    bufs[0].base = const_cast<char *>(data);
    bufs[0].len = len;
    Write<1>(bufs);
}

void Connection::Write(const char *data) {
    Write(data, strlen(data));
}

void Connection::WriteDataFrameHeader(uint8_t opcode,
                                      size_t  len,
                                      char   *headerStart) {
    DataFrameHeader header{headerStart};

    header.reset();
    header.fin(true);
    header.opcode(opcode);
    header.mask(false);
    header.rsv1(false);
    header.rsv2(false);
    header.rsv3(false);
    if (len >= 126) {
        if (len > UINT16_MAX) {
            header.len(127);
            *reinterpret_cast<uint8_t *>(headerStart + 2) = (len >> 56) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 3) = (len >> 48) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 4) = (len >> 40) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 5) = (len >> 32) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 6) = (len >> 24) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 7) = (len >> 16) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 8) = (len >> 8) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 9) = (len >> 0) & 0xFF;
        } else {
            header.len(126);
            *reinterpret_cast<uint8_t *>(headerStart + 2) = (len >> 8) & 0xFF;
            *reinterpret_cast<uint8_t *>(headerStart + 3) = (len >> 0) & 0xFF;
        }
    } else {
        header.len(len);
    }
}

auto Connection::GetDataFrameHeaderSize(size_t len) -> size_t {
    if (len >= 126) {
        if (len > UINT16_MAX) {
            return 10;
        } else {
            return 4;
        }
    } else {
        return 2;
    }
}

void Connection::OnRawSocketData(char *data, size_t len) {
    if (len == 0) {
        return;
    }
    if (!m_Socket) {
        return;
    }

    if (m_bWaitingForFirstPacket) {
        m_bWaitingForFirstPacket = false;
    }

    OnSocketData(data, len);
}

void Connection::OnSocketData(char *data, size_t len) {
    if (m_pServer == nullptr) {
        return;
    }

    // This gives us an extra byte just in case
    if (m_Buffer.size() + len + 1 >= m_pServer->max_message_size_) {
        if (m_bHasCompletedHandshake) {
            Close(1009, "Message too large");
        }

        Destroy();
        return;
    }

    // If we don't have anything stored in our class-level buffer
    // (m_Buffer), we use the buffer we received in the function arguments
    // so we don't have to perform any copying. The Bail function needs to
    // be called before we leave this function (unless we're destroying the
    // client), to copy the unused part of the buffer back to our
    // class-level buffer
    std::string_view buffer;
    bool             usingLocalBuffer = false;

    if (m_Buffer.empty()) {
        usingLocalBuffer = true;
        buffer = std::string_view(data, len);
    } else {
        usingLocalBuffer = false;

        m_Buffer.insert(m_Buffer.end(), data, data + len);
        buffer = std::string_view(m_Buffer.data(), m_Buffer.size());
    }

    auto Consume = [&](size_t amount) {
        assert(buffer.size() >= amount);
        buffer.remove_prefix(amount);
    };

    auto Bail = [&]() {
        // Copy partial HTTP headers to our buffer
        if (usingLocalBuffer) {
            if (!buffer.empty()) {
                assert(m_Buffer.empty());
                m_Buffer.insert(m_Buffer.end(),
                                buffer.data(),
                                buffer.data() + buffer.size());
            }
        } else {
            if (buffer.empty()) {
                m_Buffer.clear();
            } else if (buffer.size() != m_Buffer.size()) {
                memmove(m_Buffer.data(), buffer.data(), buffer.size());
                m_Buffer.resize(buffer.size());
            }
        }
    };

    // if (!m_bHasCompletedHandshake && m_pServer->GetAllowAlternativeProtocol()
    //     && buffer[0] == 0x00) {
    //     m_bHasCompletedHandshake = true;
    //     m_bUsingAlternativeProtocol = true;
    //     Consume(1);
    //
    //     if (!m_pServer->CheckAlternativeConnection_cb
    //         || m_pServer->CheckAlternativeConnection_cb(this)) {
    //         Destroy();
    //         return;
    //     }
    //
    //     RequestHeaders const headers;
    //     HTTPRequest          req{
    //         m_pServer,
    //         "GET",
    //         "/",
    //         m_IP,
    //         headers,
    //     };
    //
    //     if (m_pServer->ConnectionConnected_cb) {
    //         m_pServer->ConnectionConnected_cb(req);
    //     }
    // } else if (!m_bHasCompletedHandshake) {
    if (!m_bHasCompletedHandshake) {
        // HTTP headers not done yet, wait
        auto endOfHeaders = buffer.find("\r\n\r\n");
        if (endOfHeaders == std::string_view::npos) {
            return Bail();
        }

        auto MalformedRequest = [&]() {
            Write("HTTP/1.1 400 Bad Request\r\n\r\n");
            Destroy();
        };

        auto headersBuffer
            = buffer.substr(0, endOfHeaders + 4); // Include \r\n\r\n

        std::string_view method;
        std::string_view path;

        {
            auto methodEnd = headersBuffer.find(' ');

            auto endOfLine = headersBuffer.find("\r\n");
            assert(endOfLine != std::string_view::npos); // Can't fail because
                                                         // of a check above

            if (methodEnd == std::string_view::npos || methodEnd > endOfLine) {
                return MalformedRequest();
            }

            method = headersBuffer.substr(0, methodEnd);

            // Uppercase method
            std::transform(method.begin(),
                           method.end(),
                           const_cast<char *>(method.data()),
                           [](char v) -> char {
                               if (v < 0 || v >= 127) {
                                   return v;
                               }
                               return toupper(v);
                           });

            auto pathStart = methodEnd + 1;
            auto pathEnd = headersBuffer.find(' ', pathStart);

            if (pathEnd == std::string_view::npos || pathEnd > endOfLine) {
                return MalformedRequest();
            }

            path = headersBuffer.substr(pathStart, pathEnd - pathStart);

            // Skip line
            headersBuffer = headersBuffer.substr(endOfLine + 2);
        }

        RequestHeaders headers;

        for (;;) {
            auto nextLine = headersBuffer.find("\r\n");

            // This means that we have finished parsing the headers
            if (nextLine == 0) {
                break;
            }

            // This can't happen... right?
            if (nextLine == std::string_view::npos) {
                return MalformedRequest();
            }

            auto colonPos = headersBuffer.find(':');
            if (colonPos == std::string_view::npos || colonPos > nextLine) {
                return MalformedRequest();
            }

            auto key = headersBuffer.substr(0, colonPos);

            // Key to lower case
            std::transform(key.begin(),
                           key.end(),
                           const_cast<char *>(key.data()),
                           [](char v) -> char {
                               if (v < 0 || v >= 127) {
                                   return v;
                               }
                               return tolower(v);
                           });

            auto value
                = headersBuffer.substr(colonPos + 1, nextLine - (colonPos + 1));

            // Trim spaces
            while (!key.empty() && key.front() == ' ') {
                key.remove_prefix(1);
            }
            while (!key.empty() && key.back() == ' ') {
                key.remove_suffix(1);
            }
            while (!value.empty() && value.front() == ' ') {
                value.remove_prefix(1);
            }
            while (!value.empty() && value.back() == ' ') {
                value.remove_suffix(1);
            }

            headers.Set(key, value);

            headersBuffer = headersBuffer.substr(nextLine + 2);
        }

        HTTPRequest req{
            m_pServer,
            method,
            path,
            m_IP,
            headers,
        };

        {
            if (auto upgrade = headers.Get("upgrade")) {
                if (!detail::equalsi(*upgrade, "websocket")) {
                    return MalformedRequest();
                }
            } else {

                HTTPResponse res;

                if (m_pServer->HTTPRequest_cb) {
                    m_pServer->HTTPRequest_cb(req, res);
                }

                if (res.statusCode == 0) {
                    res.statusCode = 404;
                }
                if (res.statusCode < 200 || res.statusCode >= 1000) {
                    res.statusCode = 500;
                }

                const char *statusCodeText
                    = "WS28"; // Too lazy to create a table of those

                std::stringstream ss;
                ss << "HTTP/1.1 " << res.statusCode << " " << statusCodeText
                   << "\r\n";
                ss << "Connection: close\r\n";
                ss << "Content-Length: " << res.body.size() << "\r\n";

                for (auto &p : res.headers) {
                    ss << p.first << ": " << p.second << "\r\n";
                }

                ss << "\r\n";

                ss << res.body;

                std::string str = ss.str();
                Write(str.data(), str.size());

                Destroy();
                return;
            }
        }

        // WebSocket upgrades must be GET
        if (method != "GET") {
            return MalformedRequest();
        }

        auto connectionHeader = headers.Get("connection");
        if (!connectionHeader) {
            return MalformedRequest();
        }

        // Hackish, ideally we should check it's surrounded by commas (or
        // start/end of string)
        if (!detail::HeaderContains(*connectionHeader, "upgrade")) {
            return MalformedRequest();
        }

        bool sendMyVersion = false;

        auto websocketVersion = headers.Get("sec-websocket-version");
        if (!websocketVersion) {
            return MalformedRequest();
        }
        if (!detail::equalsi(*websocketVersion, "13")) {
            sendMyVersion = true;
        }

        auto websocketKey = headers.Get("sec-websocket-key");
        if (!websocketKey) {
            return MalformedRequest();
        }

        std::string securityKey = std::string(*websocketKey);

        if (m_pServer->CheckConnection_cb
            && !m_pServer->CheckConnection_cb(req)) {
            Write("HTTP/1.1 403 Forbidden\r\n\r\n");
            Destroy();
            return;
        }

        securityKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        unsigned char hash[20];
#if OPENSSL_VERSION_NUMBER <= 0x030000000L
        SHA_CTX sha1;
        SHA1_Init(&sha1);
        SHA1_Update(&sha1, securityKey.data(), securityKey.size());
        SHA1_Final(hash, &sha1);
#else
        EVP_MD_CTX *sha1 = EVP_MD_CTX_new();
        EVP_DigestInit_ex(sha1, EVP_sha1(), nullptr);
        EVP_DigestUpdate(sha1, securityKey.data(), securityKey.size());
        EVP_DigestFinal_ex(sha1, hash, nullptr);
        EVP_MD_CTX_free(sha1);
#endif
        auto solvedHash = base64_encode(hash, sizeof(hash));

        // char buf[256]; // We can use up to 101 + 27 + 28 + 1 characters,
        // and we
        //                // round up just because
        // int const bufLen
        //     = snprintf(buf,
        //                sizeof(buf),
        //                "HTTP/1.1 101 Switching Protocols\r\n"
        //                "Upgrade: websocket\r\n"
        //                "Connection: Upgrade\r\n"
        //                "%s"
        //                "Sec-WebSocket-Accept: %s\r\n\r\n",
        //
        //                sendMyVersion ? "Sec-WebSocket-Version: 13\r\n" :
        //                "", solvedHash.c_str());
        // assert(bufLen >= 0 && (size_t) bufLen < sizeof(buf));
        //
        // Write(buf, bufLen);

        auto msg
            = std::format("HTTP/1.1 101 Switching Protocols\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "{}"
                          "Sec-WebSocket-Accept: {}\r\n\r\n",

                          sendMyVersion ? "Sec-WebSocket-Version: 13\r\n" : "",
                          solvedHash);
        Write(msg.data(), msg.size());

        if (!m_Socket) {
            return; // if write failed, we're being destroyed
        }

        m_bHasCompletedHandshake = true;

        if (m_pServer->ConnectionConnected_cb) {
            m_pServer->ConnectionConnected_cb(req);
        }

        m_Buffer.clear();

        return;
    }

    detail::Corker const corker{*this};

    for (;;) {
        if (!m_Socket) {
            return; // No need to destroy even
        }

        if (m_bUsingAlternativeProtocol) {
            if (buffer.size() < 4) {
                return Bail();
            }
            uint32_t const frameLength
                = (static_cast<uint32_t>(static_cast<uint8_t>(buffer[0])))
                  | (static_cast<uint32_t>(static_cast<uint8_t>(buffer[1]))
                     << 8)
                  | (static_cast<uint32_t>(static_cast<uint8_t>(buffer[2]))
                     << 16)
                  | (static_cast<uint32_t>(static_cast<uint8_t>(buffer[3]))
                     << 24);
            if (frameLength > m_pServer->max_message_size_) {
                return Close(1002, "Too large");
            }
            if (buffer.size() < 4 + frameLength) {
                return Bail();
            }

            ProcessDataFrame(2,
                             const_cast<char *>(buffer.data()) + 4,
                             frameLength);
            Consume(4 + frameLength);
        } else { // Websockets
            // Not enough to read the header
            if (buffer.size() < 2) {
                return Bail();
            }

            DataFrameHeader header(const_cast<char *>(buffer.data()));

            if (header.rsv1() || header.rsv2() || header.rsv3()) {
                return Close(1002, "Reserved bit used");
            }

            // Connections MUST mask their headers
            if (!header.mask()) {
                return Close(1002, "Connections must mask their payload");
            }
            assert(header.mask());

            char *curPosition = const_cast<char *>(buffer.data()) + 2;

            size_t frameLength = header.len();
            if (frameLength == 126) {
                if (buffer.size() < 4) {
                    return Bail();
                }
                frameLength = (*reinterpret_cast<uint8_t *>(curPosition) << 8)
                              | (*reinterpret_cast<uint8_t *>(curPosition + 1));
                curPosition += 2;
            } else if (frameLength == 127) {
                if (buffer.size() < 10) {
                    return Bail();
                }

                frameLength
                    = (static_cast<uint64_t>(
                           *reinterpret_cast<uint8_t *>(curPosition))
                       << 56)
                      | (static_cast<uint64_t>(
                             *reinterpret_cast<uint8_t *>(curPosition + 1))
                         << 48)
                      | (static_cast<uint64_t>(
                             *reinterpret_cast<uint8_t *>(curPosition + 2))
                         << 40)
                      | (static_cast<uint64_t>(
                             *reinterpret_cast<uint8_t *>(curPosition + 3))
                         << 32)
                      | (*reinterpret_cast<uint8_t *>(curPosition + 4) << 24)
                      | (*reinterpret_cast<uint8_t *>(curPosition + 5) << 16)
                      | (*reinterpret_cast<uint8_t *>(curPosition + 6) << 8)
                      | (*reinterpret_cast<uint8_t *>(curPosition + 7) << 0);

                curPosition += 8;
            }

            auto amountLeft = buffer.size() - (curPosition - buffer.data());
            const char *maskKey = nullptr;

            { // Read mask
                if (amountLeft < 4) {
                    return Bail();
                }
                maskKey = curPosition;
                curPosition += 4;
                amountLeft -= 4;
            }

            if (frameLength > amountLeft) {
                return Bail();
            }

            { // Unmask
                for (size_t i = 0; i < (frameLength & ~3); i += 4) {
                    curPosition[i + 0] ^= maskKey[0];
                    curPosition[i + 1] ^= maskKey[1];
                    curPosition[i + 2] ^= maskKey[2];
                    curPosition[i + 3] ^= maskKey[3];
                }

                for (size_t i = frameLength & ~3; i < frameLength; ++i) {
                    curPosition[i] ^= maskKey[i % 4];
                }
            }

            if (header.opcode() >= 0x08) {
                if (!header.fin()) {
                    return Close(1002, "Control op codes can't be fragmented");
                }
                if (frameLength > 125) {
                    return Close(
                        1002,
                        "Control op codes can't be more than 125 bytes");
                }

                ProcessDataFrame(header.opcode(), curPosition, frameLength);
            } else if (!IsBuildingFrames() && header.fin()) {
                // Fast path, we received a whole frame and we don't need to
                // combine it with anything
                ProcessDataFrame(header.opcode(), curPosition, frameLength);
            } else {
                if (IsBuildingFrames()) {
                    if (header.opcode() != 0) {
                        return Close(1002, "Expected continuation frame");
                    }
                } else {
                    if (header.opcode() == 0) {
                        return Close(1002, "Unexpected continuation frame");
                    }
                    m_iFrameOpcode = header.opcode();
                }

                if (m_FrameBuffer.size() + frameLength
                    >= m_pServer->max_message_size_) {
                    return Close(1009, "Message too large");
                }

                m_FrameBuffer.insert(m_FrameBuffer.end(),
                                     curPosition,
                                     curPosition + frameLength);

                if (header.fin()) {
                    // Assemble frame

                    ProcessDataFrame(m_iFrameOpcode,
                                     m_FrameBuffer.data(),
                                     m_FrameBuffer.size());

                    m_iFrameOpcode = 0;
                    m_FrameBuffer.clear();
                }
            }

            Consume((curPosition - buffer.data()) + frameLength);
        }
    }

    // Unreachable
}

void Connection::ProcessDataFrame(uint8_t opcode, char *data, size_t len) {
    switch (opcode) {
    case 9: // Ping
        if (m_bIsClosing) {
            return;
        }
        Send(data, len, 10); // Send Pong
        break;

    case 10:
        break; // Pong

    case 8: // Close
        m_bConnectionRequestedClose = true;
        if (m_bIsClosing) {
            Destroy();
        } else {

            if (len == 1) {
                Close(1002, "Incomplete close code");
                return;
            } else if (len >= 2) {
                bool           invalid = false;
                uint16_t const code = (static_cast<uint8_t>(data[0]) << 8)
                                      | static_cast<uint8_t>(data[1]);
                if (code < 1000 || code >= 5000) {
                    invalid = true;
                }

                switch (code) {
                case 1004:
                case 1005:
                case 1006:
                case 1015:
                    invalid = true;
                default:;
                }

                if (invalid) {
                    Close(1002, "Invalid close code");
                    return;
                }

                if (len > 2 && !IsValidUTF8(data + 2, len - 2)) {
                    Close(1002, "Close reason is not UTF-8");
                    return;
                }
            }

            // Copy close message
            m_bIsClosing = true;

            char header[MAX_HEADER_SIZE];
            WriteDataFrameHeader(8, len, header);

            uv_buf_t bufs[2];
            bufs[0].base = header;
            bufs[0].len = GetDataFrameHeaderSize(len);
            bufs[1].base = data;
            bufs[1].len = len;

            Write<2>(bufs);

            // We always close the tcp connection on our side, as allowed
            // in 7.1.1
            Destroy();
        }
        break;

    case 1: // Text
    case 2: // Binary
        if (m_bIsClosing) {
            return;
        }
        if (opcode == 1 && !IsValidUTF8(data, len)) {
            return Close(1007, "Invalid UTF-8 in text frame");
        }

        if (m_pServer->ConnectionData_cb) {
            m_pServer->ConnectionData_cb(this, data, len, opcode);
        }
        break;

    default:
        return Close(1002, "Unknown op code");
    }
}

void Connection::Close(uint16_t code, const char *reason, size_t reasonLen) {
    if (m_bIsClosing) {
        return;
    }

    m_bIsClosing = true;

    if (!m_bUsingAlternativeProtocol) {
        char coded[2];
        coded[0] = (code >> 8) & 0xFF;
        coded[1] = (code >> 0) & 0xFF;

        if (reason == nullptr) {
            Send(coded, sizeof(coded), 8);
        } else {
            if (reasonLen == static_cast<size_t>(-1)) {
                reasonLen = strlen(reason);
            }

            char header[MAX_HEADER_SIZE];
            WriteDataFrameHeader(8, 2 + reasonLen, header);

            uv_buf_t bufs[2];
            bufs[0].base = header;
            bufs[0].len = GetDataFrameHeaderSize(2 + reasonLen);
            bufs[1].base = const_cast<char *>(reason);
            bufs[1].len = reasonLen;

            Write<2>(bufs);
        }
    }

    // We always close the tcp connection on our side, as allowed in 7.1.1
    Destroy();
}

void Connection::Send(const char *data, size_t len, uint8_t opcode) {
    if (!m_Socket) {
        return;
    }

    if (m_bUsingAlternativeProtocol) {
        auto const len32 = static_cast<uint32_t>(len);
        uint8_t    header[4];
        header[0] = (len32 >> 0) & 0xFF;
        header[1] = (len32 >> 8) & 0xFF;
        header[2] = (len32 >> 16) & 0xFF;
        header[3] = (len32 >> 24) & 0xFF;

        uv_buf_t bufs[2];
        bufs[0].base = reinterpret_cast<char *>(header);
        bufs[0].len = 4;
        bufs[1].base = const_cast<char *>(data);
        bufs[1].len = len;

        Write<2>(bufs);
    } else {
        char header[MAX_HEADER_SIZE];
        WriteDataFrameHeader(opcode, len, header);

        uv_buf_t bufs[2];
        bufs[0].base = header;
        bufs[0].len = GetDataFrameHeaderSize(len);
        bufs[1].base = const_cast<char *>(data);
        bufs[1].len = len;

        Write<2>(bufs);
    }
}

void Connection::Cork(bool v) {
    if (!m_Socket) {
        return;
    }

#if defined(TCP_CORK) || defined(TCP_NOPUSH)

    int        enable = static_cast<int>(v);
    uv_os_fd_t fd = 0;
    uv_fileno(reinterpret_cast<uv_handle_t *>(m_Socket.get()), &fd);

    // Shamelessly copied from uWebSockets
#if defined(TCP_CORK)
    // Linux
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &enable, sizeof(int));
#elif defined(TCP_NOPUSH)
    // Mac OS X & FreeBSD
    setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &enable, sizeof(int));

    // MacOS needs this to flush the messages out
    if (!enable) {
        ::send(fd, "", 0, 0);
    }
#endif

#endif
}

//===============================================================================

auto main() -> int {
    Server server{uv_default_loop()};

    server.SetConnectionConnectedCallback([](HTTPRequest &) {
        LOG_DEBUG("Connection connected");
    });

    server.SetConnectionDisconnectedCallback([]() {
        LOG_DEBUG("Connection disconnected");
    });

    server.SetConnectionDataCallback(
        [](Connection *conn, char *data, size_t len, int opcode) {
            LOG_DEBUG("Received: {}", std::string_view{data, len});
            conn->Send(data, len, opcode);
        });

    server.SetHTTPCallback([](HTTPRequest &req, HTTPResponse &resp) {
        std::stringstream ss;
        ss << "Hi, you issued a " << req.method << " to " << req.path << "\r\n";
        ss << "Headers:\r\n";

        req.headers.ForEach([&](std::string_view key, std::string_view value) {
            ss << key << ": " << value << "\r\n";
        });

        resp.send(ss.str());
    });

    server.Listen(3000);

    LOG_DEBUG("Listening on :3000 ...");
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
}