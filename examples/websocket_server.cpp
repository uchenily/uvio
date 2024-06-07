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
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <uv.h>
#include <vector>

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

static inline auto is_base64(unsigned char c) -> bool {
    return ((isalnum(c) != 0) || (c == '+') || (c == '/'));
}

auto base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len)
    -> std::string {
    std::string   ret;
    int           i = 0;
    int           j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while ((in_len--) != 0u) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4)
                              + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2)
                              + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++) {
                ret += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1]
            = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2]
            = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) {
            ret += base64_chars[char_array_4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

auto base64_decode(std::string const &encoded_string) -> std::string {
    int           in_len = static_cast<int>(encoded_string.size());
    int           i = 0;
    int           j = 0;
    int           in_ = 0;
    unsigned char char_array_4[4];
    unsigned char char_array_3[3];
    std::string   ret;

    while (((in_len--) != 0) && (encoded_string[in_] != '=')
           && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = static_cast<unsigned char>(
                    base64_chars.find(char_array_4[i]));
            }

            char_array_3[0]
                = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4)
                              + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret += char_array_3[i];
            }
            i = 0;
        }
    }

    if (i != 0) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        for (j = 0; j < 4; j++) {
            char_array_4[j] = static_cast<unsigned char>(
                base64_chars.find(char_array_4[j]));
        }

        char_array_3[0]
            = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1]
            = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret += char_array_3[j];
        }
    }

    return ret;
}

// Ported from https://github.com/darrenjs/openssl_examples
// MIT licensed
class TLS {
    enum SSLStatus { SSLSTATUS_OK, SSLSTATUS_WANT_IO, SSLSTATUS_FAIL };

public:
    TLS(SSL_CTX *ctx, bool server = true, const char *hostname = nullptr)
        : m_SSL(SSL_new(ctx))
        , m_ReadBIO(BIO_new(BIO_s_mem()))
        , m_WriteBIO(BIO_new(BIO_s_mem())) {

        if (server) {
            SSL_set_accept_state(m_SSL);
        } else {
            SSL_set_connect_state(m_SSL);
        }

        if (!server && hostname) {
            SSL_set_tlsext_host_name(m_SSL, hostname);
        }
        SSL_set_bio(m_SSL, m_ReadBIO, m_WriteBIO);

        if (!server) {
            DoSSLHandhake();
        }
    }

    ~TLS() {
        SSL_free(m_SSL);
    }

    TLS(const TLS &other) = delete;
    auto operator=(const TLS &other) -> TLS & = delete;

    // Helper to setup SSL, you still need to create the context
    static void InitSSL() {
        static std::once_flag f;
        std::call_once(f, []() {
            SSL_library_init();
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER < 0x30000000L || defined(LIBRESSL_VERSION_NUMBER)
            ERR_load_BIO_strings();
#endif
            ERR_load_crypto_strings();
        });
    }

    // Writes unencrypted bytes to be encrypted and sent out
    // If this returns false, the connection must be closed
    auto Write(const char *buf, size_t len) -> bool {
        m_EncryptBuf.insert(m_EncryptBuf.end(), buf, buf + len);
        return DoEncrypt();
    }

    // Process raw bytes received from the other side
    // If this returns false, the connection must be closed
    template <typename F>
    auto ReceivedData(const char *src, size_t len, const F &f) -> bool {
        int n = 0;
        while (len > 0) {
            n = BIO_write(m_ReadBIO, src, len);

            // Assume bio write failure is unrecoverable
            if (n <= 0) {
                return false;
            }

            src += n;
            len -= n;

            if (!SSL_is_init_finished(m_SSL)) {
                if (DoSSLHandhake() == SSLSTATUS_FAIL) {
                    return false;
                }
                if (!SSL_is_init_finished(m_SSL)) {
                    return true;
                }
            }

            ERR_clear_error();
            do {
                char buf[4096];
                n = SSL_read(m_SSL, buf, sizeof buf);
                if (n > 0) {
                    f(buf, static_cast<size_t>(n));
                }
            } while (n > 0);

            auto status = GetSSLStatus(n);
            if (status == SSLSTATUS_WANT_IO) {
                do {
                    char buf[4096];
                    n = BIO_read(m_WriteBIO, buf, sizeof(buf));
                    if (n > 0) {
                        QueueEncrypted(buf, n);
                    } else if (!BIO_should_retry(m_WriteBIO)) {
                        return false;
                    }
                } while (n > 0);
            } else if (status == SSLSTATUS_FAIL) {
                return false;
            }
        }

        return true;
    }

    template <typename F>
    void ForEachPendingWrite(const F &f) {
        // If the callback does something crazy like calling Write inside of it
        // We need to handle this carefully, thus the swap.
        for (;;) {
            if (m_WriteBuf.empty()) {
                return;
            }

            std::vector<char> buf;
            std::swap(buf, m_WriteBuf);

            f(buf.data(), buf.size());
        }
    }

    auto IsHandshakeFinished() -> bool {
        return SSL_is_init_finished(m_SSL) != 0;
    }

private:
    auto GetSSLStatus(int n) -> SSLStatus {
        switch (SSL_get_error(m_SSL, n)) {
        case SSL_ERROR_NONE:
            return SSLSTATUS_OK;

        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_READ:
            return SSLSTATUS_WANT_IO;

        case SSL_ERROR_ZERO_RETURN:
        case SSL_ERROR_SYSCALL:
        default:
            return SSLSTATUS_FAIL;
        }
    }

    void QueueEncrypted(const char *buf, size_t len) {
        m_WriteBuf.insert(m_WriteBuf.end(), buf, buf + len);
    }

    auto DoEncrypt() -> bool {
        if (SSL_is_init_finished(m_SSL) == 0) {
            return true;
        }

        int n = 0;

        while (!m_EncryptBuf.empty()) {
            ERR_clear_error();
            n = SSL_write(m_SSL,
                          m_EncryptBuf.data(),
                          static_cast<int>(m_EncryptBuf.size()));

            if (GetSSLStatus(n) == SSLSTATUS_FAIL) {
                return false;
            }

            if (n > 0) {
                // Consume bytes
                m_EncryptBuf.erase(m_EncryptBuf.begin(),
                                   m_EncryptBuf.begin() + n);

                // Write them out
                do {
                    char buf[4096];
                    n = BIO_read(m_WriteBIO, buf, sizeof buf);
                    if (n > 0) {
                        QueueEncrypted(buf, n);
                    } else if (!BIO_should_retry(m_WriteBIO)) {
                        return false;
                    }
                } while (n > 0);
            }
        }

        return true;
    }

    auto DoSSLHandhake() -> SSLStatus {
        ERR_clear_error();
        SSLStatus const status = GetSSLStatus(SSL_do_handshake(m_SSL));

        // Did SSL request to write bytes?
        if (status == SSLSTATUS_WANT_IO) {
            int n = 0;
            do {
                char buf[4096];
                n = BIO_read(m_WriteBIO, buf, sizeof buf);

                if (n > 0) {
                    QueueEncrypted(buf, n);
                } else if (!BIO_should_retry(m_WriteBIO)) {
                    return SSLSTATUS_FAIL;
                }

            } while (n > 0);
        }

        return status;
    }

    std::vector<char> m_EncryptBuf; // Bytes waiting to be encrypted
    std::vector<char> m_WriteBuf;   // Bytes waiting to be written to the socket

    SSL *m_SSL;
    BIO *m_ReadBIO;
    BIO *m_WriteBIO;
};

class Client;
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

using SocketHandle = std::unique_ptr<uv_tcp_t, detail::SocketDeleter>;

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

    friend class Client;
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

    friend class Client;
};

class Client {
    enum { MAX_HEADER_SIZE = 10 };
    enum : unsigned char { NO_FRAMES = 0 };

public:
    ~Client();

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

    inline auto IsSecure() -> bool {
        return m_pTLS != nullptr;
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

    inline auto HasClientRequestedClose() const -> bool {
        return m_bClientRequestedClose;
    }

private:
    struct DataFrame {
        uint8_t                 opcode;
        std::unique_ptr<char[]> data;
        size_t                  len;
    };

    Client(Server *server, SocketHandle socket);

    Client(const Client &other) = delete;
    auto operator=(Client &other) -> Client & = delete;

    auto GetDataFrameHeaderSize(size_t len) -> size_t;
    void WriteDataFrameHeader(uint8_t opcode, size_t len, char *headerStart);
    void EncryptAndWrite(const char *data, size_t len);

    void OnRawSocketData(char *data, size_t len);
    void OnSocketData(char *data, size_t len);
    void ProcessDataFrame(uint8_t opcode, char *data, size_t len);

    void InitSecure();
    void FlushTLS();

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

    Server      *m_pServer;
    SocketHandle m_Socket;
    void        *m_pUserData = nullptr;
    bool         m_bWaitingForFirstPacket = true;
    bool         m_bHasCompletedHandshake = false;
    bool         m_bIsClosing = false;
    bool         m_bUsingAlternativeProtocol = false;
    bool         m_bClientRequestedClose = false;
    char         m_IP[46];

    std::unique_ptr<TLS> m_pTLS;

    std::vector<char> m_Buffer;

    uint8_t           m_iFrameOpcode = NO_FRAMES;
    std::vector<char> m_FrameBuffer;

    friend class Server;
    friend struct detail::Corker;
    friend class std::unique_ptr<Client>;
};

class Server {
public:
    using CheckTCPConnectionFn = bool (*)(std::string_view, bool);
    using CheckConnectionFn = bool (*)(Client *, HTTPRequest &);
    using CheckAlternativeConnectionFn = bool (*)(Client *);
    using ClientConnectedFn = void (*)(Client *, HTTPRequest &);
    using ClientDisconnectedFn = void (*)(Client *);
    using ClientDataFn = void (*)(Client *, char *, size_t, int);
    using HTTPRequestFn = void (*)(HTTPRequest &, HTTPResponse &);

public:
    // Note: By default, this listens on both ipv4 and ipv6
    // Note: if you provide a SSL_CTX, this server will listen to *BOTH* secure
    // and insecure connections at that port,
    //       sniffing the first byte to figure out whether it's secure or not
    Server(uv_loop_t *loop, SSL_CTX *ctx = nullptr);
    Server(const Server &other) = delete;
    auto operator=(const Server &other) -> Server & = delete;
    ~Server();

    auto Listen(int port, bool ipv4Only = false) -> bool;
    void StopListening();
    void DestroyClients();

    // This callback is called when we know whether a TCP connection wants a
    // secure connection or not, once we receive the very first byte from the
    // client
    void SetCheckTCPConnectionCallback(CheckTCPConnectionFn v) {
        m_fnCheckTCPConnection = v;
    }

    // This callback is called when the client is trying to connect using
    // websockets By default, for safety, this checks the Origin and makes sure
    // it matches the Host It's likely you wanna change this check if your
    // websocket server is in a different domain.
    void SetCheckConnectionCallback(CheckConnectionFn v) {
        m_fnCheckConnection = v;
    }

    // This is called instead of CheckConnection for connections using the
    // alternative protocol (if enabled)
    void SetCheckAlternativeConnectionCallback(CheckAlternativeConnectionFn v) {
        m_fnCheckAlternativeConnection = v;
    }

    // This callback is called when a client establishes a connection (after
    // websocket handshake) This is paired with the disconnected callback
    void SetClientConnectedCallback(ClientConnectedFn v) {
        m_fnClientConnected = v;
    }

    // This callback is called when a client disconnects
    // This is paired with the connected callback, and will *always* be called
    // for clients that called the other callback Note that clients grab this
    // value when you call Destroy on them, so changing this after clients are
    // connected might lead to weird results. In practice, just set it once and
    // forget about it.
    void SetClientDisconnectedCallback(ClientDisconnectedFn v) {
        m_fnClientDisconnected = v;
    }

    // This callback is called when the client receives a data frame
    // Note that both text and binary op codes end up here
    void SetClientDataCallback(ClientDataFn v) {
        m_fnClientData = v;
    }

    // This callback is called when a normal http request is received
    // If you don't send anything in response, the status code is 404
    // If you send anything in response without setting a specific status code,
    // it will be 200 Connections that call this callback never lead to a
    // connection
    void SetHTTPCallback(HTTPRequestFn v) {
        m_fnHTTPRequest = v;
    }

    auto GetSSLContext() const -> SSL_CTX * {
        return m_pSSLContext;
    }

    inline void SetUserData(void *v) {
        m_pUserData = v;
    }
    inline auto GetUserData() const -> void * {
        return m_pUserData;
    }

    // Adjusts how much we're willing to accept from clients
    // Note: this can only be set while we don't have clients (preferably before
    // listening)
    inline void SetMaxMessageSize(size_t v) {
        assert(m_Clients.empty());
        m_iMaxMessageSize = v;
    }

    // Alternative protocol means that the client sends a 0x00, and we skip all
    // websocket protocol This means clients don't call CheckConnection, and
    // they receive an empty request header in the connection callback Opcode is
    // always binary In the alternative protocol, clients and servers send the
    // packet length as a Little Endian uint32, then its contents
    inline void SetAllowAlternativeProtocol(bool v) {
        m_bAllowAlternativeProtocol = v;
    }
    inline auto GetAllowAlternativeProtocol() const -> bool {
        return m_bAllowAlternativeProtocol;
    }

    void Ref() const {
        if (m_Server) {
            uv_ref(reinterpret_cast<uv_handle_t *>(m_Server.get()));
        }
    }
    void Unref() const {
        if (m_Server) {
            uv_unref(reinterpret_cast<uv_handle_t *>(m_Server.get()));
        }
    }

    // private:
    // TODO(x)
public:
    void OnConnection(uv_stream_t *server, int status);

    void NotifyClientInit(Client *client, HTTPRequest &req) const {
        if (m_fnClientConnected) {
            m_fnClientConnected(client, req);
        }
    }

    auto NotifyClientPreDestroyed(Client *client) -> std::unique_ptr<Client>;

    void
    NotifyClientData(Client *client, char *data, size_t len, int opcode) const {
        if (m_fnClientData) {
            m_fnClientData(client, data, len, opcode);
        }
    }

    uv_loop_t                           *m_pLoop;
    SocketHandle                         m_Server;
    SSL_CTX                             *m_pSSLContext;
    void                                *m_pUserData = nullptr;
    std::vector<std::unique_ptr<Client>> m_Clients;
    bool                                 m_bAllowAlternativeProtocol = false;

    CheckTCPConnectionFn         m_fnCheckTCPConnection = nullptr;
    CheckConnectionFn            m_fnCheckConnection = nullptr;
    CheckAlternativeConnectionFn m_fnCheckAlternativeConnection = nullptr;
    ClientConnectedFn            m_fnClientConnected = nullptr;
    ClientDisconnectedFn         m_fnClientDisconnected = nullptr;
    ClientDataFn                 m_fnClientData = nullptr;
    HTTPRequestFn                m_fnHTTPRequest = nullptr;

    size_t m_iMaxMessageSize = static_cast<size_t>(16 * 1024);

    friend class Client;
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
                // We have a match... if the header ends here, or has a comma
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
    Client &client;

    Corker(Client &client)
        : client(client) {
        client.Cork(true);
    }
    ~Corker() {
        client.Cork(false);
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

Client::Client(Server *server, SocketHandle socket)
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

            // Remove this prefix if it exists, it means that we actually have a
            // ipv4
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
            auto client = static_cast<Client *>(stream->data);

            if (client != nullptr) {
                if (nread < 0) {
                    client->Destroy();
                } else if (nread > 0) {
                    client->OnRawSocketData(buf->base,
                                            static_cast<size_t>(nread));
                }
            }

            if (buf != nullptr) {
                delete[] buf->base;
            }
        });
}

Client::~Client() {
    assert(!m_Socket);
}

void Client::Destroy() {
    if (!m_Socket) {
        return;
    }

    Cork(false);

    m_Socket->data = nullptr;

    auto myself = m_pServer->NotifyClientPreDestroyed(this);

    struct ShutdownRequest : uv_shutdown_t {
        SocketHandle                 socket;
        std::unique_ptr<Client>      client;
        Server::ClientDisconnectedFn cb;
    };

    auto req = new ShutdownRequest();
    req->socket = std::move(m_Socket);
    req->client = std::move(myself);
    req->cb = m_pServer->m_fnClientDisconnected;

    m_pServer = nullptr;

    static auto cb = [](uv_shutdown_t *reqq, int) {
        auto req = (ShutdownRequest *) reqq;

        if (req->cb && req->client->m_bHasCompletedHandshake) {
            req->cb(req->client.get());
        }

        delete req;
    };

    if (uv_shutdown(req, reinterpret_cast<uv_stream_t *>(req->socket.get()), cb)
        != 0) {
        // Shutdown failed, but we have to delay the destruction to the next
        // event loop
        auto timer = new uv_timer_t;
        uv_timer_init(req->socket->loop, timer);
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
void Client::WriteRaw(uv_buf_t bufs[N]) {
    if (!m_Socket) {
        return;
    }

    // Try to write without allocating memory first, if that doesn't work, we
    // call WriteRawQueue
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

void Client::WriteRawQueue(std::unique_ptr<char[]> data, size_t len) {
    if (!m_Socket) {
        return;
    }

    struct CustomWriteRequest {
        uv_write_t              req;
        Client                 *client;
        std::unique_ptr<char[]> data;
    };

    uv_buf_t buf;
    buf.base = data.get();
    buf.len = len;

    auto request = new CustomWriteRequest();
    request->client = this;
    request->data = std::move(data);

    if (uv_write(&request->req,
                 reinterpret_cast<uv_stream_t *>(m_Socket.get()),
                 &buf,
                 1,
                 [](uv_write_t *req, int status) {
                     auto request = reinterpret_cast<CustomWriteRequest *>(req);

                     if (status < 0) {
                         request->client->Destroy();
                     }

                     delete request;
                 })
        != 0) {
        delete request;
        Destroy();
    }
}

template <size_t N>
void Client::Write(uv_buf_t bufs[N]) {
    if (!m_Socket) {
        return;
    }
    if (IsSecure()) {
        for (size_t i = 0; i < N; ++i) {
            if (!m_pTLS->Write(bufs[i].base, bufs[i].len)) {
                return Destroy();
            }
        }
        FlushTLS();
    } else {
        WriteRaw<N>(bufs);
    }
}

void Client::Write(const char *data, size_t len) {
    uv_buf_t bufs[1];
    bufs[0].base = const_cast<char *>(data);
    bufs[0].len = len;
    Write<1>(bufs);
}

void Client::Write(const char *data) {
    Write(data, strlen(data));
}

void Client::WriteDataFrameHeader(uint8_t opcode,
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

auto Client::GetDataFrameHeaderSize(size_t len) -> size_t {
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

void Client::OnRawSocketData(char *data, size_t len) {
    if (len == 0) {
        return;
    }
    if (!m_Socket) {
        return;
    }

    if (m_bWaitingForFirstPacket) {
        m_bWaitingForFirstPacket = false;

        assert(!IsSecure());

        if (m_pServer->GetSSLContext() != nullptr
            && (data[0] == 0x16 || static_cast<uint8_t>(data[0]) == 0x80)) {
            if (m_pServer->m_fnCheckTCPConnection
                && !m_pServer->m_fnCheckTCPConnection(GetIP(), true)) {
                return Destroy();
            }

            InitSecure();
        } else {
            if (m_pServer->m_fnCheckTCPConnection
                && !m_pServer->m_fnCheckTCPConnection(GetIP(), false)) {
                return Destroy();
            }
        }
    }

    if (IsSecure()) {
        if (!m_pTLS->ReceivedData(data, len, [&](char *data, size_t len) {
                OnSocketData(data, len);
            })) {
            return Destroy();
        }

        FlushTLS();
    } else {
        OnSocketData(data, len);
    }
}

void Client::OnSocketData(char *data, size_t len) {
    if (m_pServer == nullptr) {
        return;
    }

    // This gives us an extra byte just in case
    if (m_Buffer.size() + len + 1 >= m_pServer->m_iMaxMessageSize) {
        if (m_bHasCompletedHandshake) {
            Close(1009, "Message too large");
        }

        Destroy();
        return;
    }

    // If we don't have anything stored in our class-level buffer (m_Buffer),
    // we use the buffer we received in the function arguments so we don't have
    // to perform any copying. The Bail function needs to be called before we
    // leave this function (unless we're destroying the client), to copy the
    // unused part of the buffer back to our class-level buffer
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

    if (!m_bHasCompletedHandshake && m_pServer->GetAllowAlternativeProtocol()
        && buffer[0] == 0x00) {
        m_bHasCompletedHandshake = true;
        m_bUsingAlternativeProtocol = true;
        Consume(1);

        if (!m_pServer->m_fnCheckAlternativeConnection
            || m_pServer->m_fnCheckAlternativeConnection(this)) {
            Destroy();
            return;
        }

        RequestHeaders const headers;
        HTTPRequest          req{
            m_pServer,
            "GET",
            "/",
            m_IP,
            headers,
        };

        m_pServer->NotifyClientInit(this, req);
    } else if (!m_bHasCompletedHandshake) {
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

                if (m_pServer->m_fnHTTPRequest) {
                    m_pServer->m_fnHTTPRequest(req, res);
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

        if (m_pServer->m_fnCheckConnection
            && !m_pServer->m_fnCheckConnection(this, req)) {
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

        // char buf[256]; // We can use up to 101 + 27 + 28 + 1 characters, and
        // we
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
        //                sendMyVersion ? "Sec-WebSocket-Version: 13\r\n" : "",
        //                solvedHash.c_str());
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

        m_pServer->NotifyClientInit(this, req);

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
            if (frameLength > m_pServer->m_iMaxMessageSize) {
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

            // Clients MUST mask their headers
            if (!header.mask()) {
                return Close(1002, "Clients must mask their payload");
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
                    >= m_pServer->m_iMaxMessageSize) {
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

void Client::ProcessDataFrame(uint8_t opcode, char *data, size_t len) {
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
        m_bClientRequestedClose = true;
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

        m_pServer->NotifyClientData(this, data, len, opcode);
        break;

    default:
        return Close(1002, "Unknown op code");
    }
}

void Client::Close(uint16_t code, const char *reason, size_t reasonLen) {
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

void Client::Send(const char *data, size_t len, uint8_t opcode) {
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

void Client::InitSecure() {
    m_pTLS = std::make_unique<TLS>(m_pServer->GetSSLContext());
}

void Client::FlushTLS() {
    assert(m_pTLS != nullptr);
    m_pTLS->ForEachPendingWrite([&](const char *data, size_t len) {
        uv_buf_t bufs[1];
        bufs[0].base = const_cast<char *>(data);
        bufs[0].len = len;
        WriteRaw<1>(bufs);
    });
}

void Client::Cork(bool v) {
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

Server::Server(uv_loop_t *loop, SSL_CTX *ctx)
    : m_pLoop(loop)
    , m_pSSLContext(ctx) {

    m_fnCheckConnection = [](Client *, HTTPRequest &req) -> bool {
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

auto Server::Listen(int port, bool ipv4Only) -> bool {
    if (m_Server) {
        return false;
    }

    signal(SIGPIPE, SIG_IGN);

    auto server = SocketHandle{new uv_tcp_t};
    uv_tcp_init_ex(m_pLoop, server.get(), ipv4Only ? AF_INET : AF_INET6);
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
    int const r = uv_fileno(reinterpret_cast<uv_handle_t *>(server.get()), &fd);
    (void) r;
    assert(r == 0);
    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if (uv_tcp_bind(server.get(), reinterpret_cast<struct sockaddr *>(&addr), 0)
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

    m_Server = std::move(server);
    return true;
}

void Server::StopListening() {
    // Just in case we have more logic in the future
    if (!m_Server) {
        return;
    }

    m_Server.reset();
}

void Server::DestroyClients() {
    // Clients will erase themselves from this vector
    while (!m_Clients.empty()) {
        m_Clients.back()->Destroy();
    }
}

Server::~Server() {
    StopListening();
    DestroyClients();
}

void Server::OnConnection(uv_stream_t *server, int status) {
    if (status < 0) {
        return;
    }

    SocketHandle socket{new uv_tcp_t};
    uv_tcp_init(m_pLoop, socket.get());

    socket->data = nullptr;

    if (uv_accept(server, reinterpret_cast<uv_stream_t *>(socket.get())) == 0) {
        auto client = new Client(this, std::move(socket));
        m_Clients.emplace_back(client);

        // If for whatever reason uv_tcp_getpeername failed (happens...
        // somehow?)
        if (client->GetIP()[0] == '\0') {
            client->Destroy();
        }
    }
}

auto Server::NotifyClientPreDestroyed(Client *client)
    -> std::unique_ptr<Client> {
    for (auto it = m_Clients.begin(); it != m_Clients.end(); ++it) {
        if (it->get() == client) {
            std::unique_ptr<Client> r = std::move(*it);
            *it = std::move(m_Clients.back());
            m_Clients.pop_back();
            return r;
        }
    }

    assert(false);
    return {};
}

//===============================================================================

auto main() -> int {
    static intptr_t userID = 0;

    Server s{uv_default_loop()};

    // I recommend against setting these limits, they're way too high and allow
    // easy DDoSes. Use the default settings. These are just here to pass tests
    s.SetMaxMessageSize(static_cast<size_t>(256 * 1024 * 1024)); // 256 MB

    s.SetClientConnectedCallback([](Client *client, HTTPRequest &) {
        client->SetUserData((void *) ++userID);
        LOG_DEBUG("Client {} connected", userID);
    });

    s.SetClientDisconnectedCallback([](Client *client) {
        LOG_DEBUG("Client {} disconnected", client->GetUserData());
    });

    s.SetClientDataCallback(
        [](Client *client, char *data, size_t len, int opcode) {
            LOG_DEBUG("Received: {}", std::string_view{data, len});
            client->Send(data, len, opcode);
        });

    s.SetHTTPCallback([](HTTPRequest &req, HTTPResponse &res) {
        std::stringstream ss;
        ss << "Hi, you issued a " << req.method << " to " << req.path << "\r\n";
        ss << "Headers:\r\n";

        req.headers.ForEach([&](std::string_view key, std::string_view value) {
            ss << key << ": " << value << "\r\n";
        });

        res.send(ss.str());
    });

    // uv_timer_t timer;
    // uv_timer_init(uv_default_loop(), &timer);
    // timer.data = &s;
    // uv_timer_start(
    //     &timer,
    //     [](uv_timer_t *timer) {
    //         // if (quit != 0) {
    //         puts("Waiting for clients to disconnect, send another SIGINT "
    //              "to force quit");
    //         auto &s = *static_cast<Server *>(timer->data);
    //         s.StopListening();
    //         uv_timer_stop(timer);
    //         uv_close(reinterpret_cast<uv_handle_t *>(timer), nullptr);
    //         // }
    //     },
    //     10,
    //     10);
    //
    s.Listen(3000);

    LOG_DEBUG("Listening on :3000 ...");
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
}
