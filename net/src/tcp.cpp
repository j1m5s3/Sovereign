// TCP links: Winsock on Windows, BSD sockets elsewhere. Non-blocking after connect; partial
// reads and writes are buffered until a whole frame is in.
#include "sovereign_net/tcp.h"

#include <cstring>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kBadSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kBadSocket = -1;
#endif

namespace sov::net {

namespace {
constexpr uint32_t kMaxFrame = 64u << 20;  // a whole save fits
#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;  // a closed peer must not raise SIGPIPE
#else
constexpr int kSendFlags = 0;
#endif

#ifdef _WIN32
struct WinsockInit {
    bool ok = false;
    WinsockInit() {
        WSADATA d;
        ok = WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }
    ~WinsockInit() {
        if (ok) WSACleanup();
    }
};
bool startSockets() {
    static WinsockInit init;
    return init.ok;
}
void closeSocket(SocketHandle s) { closesocket(s); }
bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
bool setNonBlocking(SocketHandle s) {
    u_long on = 1;
    return ioctlsocket(s, FIONBIO, &on) == 0;
}
#else
bool startSockets() { return true; }
void closeSocket(SocketHandle s) { ::close(s); }
bool wouldBlock() { return errno == EWOULDBLOCK || errno == EAGAIN; }
bool setNonBlocking(SocketHandle s) { return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == 0; }
#endif

void noDelay(SocketHandle s) {
    int on = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on);
}

class TcpLink : public Link {
public:
    explicit TcpLink(SocketHandle s) : s_(s) {
        setNonBlocking(s_);
        noDelay(s_);
    }
    ~TcpLink() override { close(); }

    bool send(const std::vector<uint8_t>& m) override {
        if (s_ == kBadSocket) return false;
        const uint32_t n = static_cast<uint32_t>(m.size());
        for (int i = 0; i < 4; ++i) out_.push_back(static_cast<uint8_t>(n >> (8 * i)));
        out_.insert(out_.end(), m.begin(), m.end());
        flush();
        return s_ != kBadSocket;
    }

    bool receive(std::vector<uint8_t>& m) override {
        flush();
        pump();
        if (in_.size() < 4) return false;
        const uint32_t n = static_cast<uint32_t>(in_[0]) | static_cast<uint32_t>(in_[1]) << 8 | static_cast<uint32_t>(in_[2]) << 16 |
                           static_cast<uint32_t>(in_[3]) << 24;
        if (n > kMaxFrame) {
            close();
            return false;
        }
        if (in_.size() < 4 + static_cast<size_t>(n)) return false;
        m.assign(in_.begin() + 4, in_.begin() + 4 + static_cast<std::ptrdiff_t>(n));
        in_.erase(in_.begin(), in_.begin() + 4 + static_cast<std::ptrdiff_t>(n));
        return true;
    }

    bool connected() const override { return s_ != kBadSocket; }

    void close() override {
        if (s_ != kBadSocket) closeSocket(s_);
        s_ = kBadSocket;
    }

private:
    void flush() {
        while (s_ != kBadSocket && !out_.empty()) {
            const int n = static_cast<int>(::send(s_, reinterpret_cast<const char*>(out_.data()), static_cast<int>(out_.size()), kSendFlags));
            if (n > 0) {
                out_.erase(out_.begin(), out_.begin() + n);
                continue;
            }
            if (n < 0 && wouldBlock()) return;
            close();
        }
    }
    void pump() {
        char buf[16384];
        while (s_ != kBadSocket) {
            const int n = static_cast<int>(::recv(s_, buf, static_cast<int>(sizeof buf), 0));
            if (n > 0) {
                in_.insert(in_.end(), buf, buf + n);
                continue;
            }
            if (n < 0 && wouldBlock()) return;
            close();  // 0: the other side closed; <0: an error
        }
    }

    SocketHandle s_;
    std::vector<uint8_t> in_, out_;
};
}  // namespace

struct TcpListener::Impl {
    SocketHandle s = kBadSocket;
    int port = 0;
};

TcpListener::TcpListener(int port, const std::string& bindAddress) : impl_(std::make_unique<Impl>()) {
    if (!startSockets()) return;
    SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kBadSocket) return;
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof on);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, bindAddress.c_str(), &a.sin_addr) != 1 || bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 ||
        listen(s, 8) != 0 || !setNonBlocking(s)) {
        closeSocket(s);
        return;
    }
    socklen_t len = sizeof a;
    getsockname(s, reinterpret_cast<sockaddr*>(&a), &len);
    impl_->s = s;
    impl_->port = ntohs(a.sin_port);
}

TcpListener::~TcpListener() {
    if (impl_->s != kBadSocket) closeSocket(impl_->s);
}

bool TcpListener::ok() const { return impl_->s != kBadSocket; }
int TcpListener::port() const { return impl_->port; }

std::unique_ptr<Link> TcpListener::accept() {
    if (impl_->s == kBadSocket) return nullptr;
    const SocketHandle c = ::accept(impl_->s, nullptr, nullptr);
    if (c == kBadSocket) return nullptr;
    return std::make_unique<TcpLink>(c);
}

std::unique_ptr<Link> tcpConnect(const std::string& host, int port, int timeoutSeconds) {
    if (!startSockets()) return nullptr;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found) return nullptr;
    SocketHandle s = kBadSocket;
    for (addrinfo* a = found; a && s == kBadSocket; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == kBadSocket) continue;
        // Connect without blocking, then wait for it with select (so a dead address times out).
        setNonBlocking(s);
        const int r = connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen));
        bool ok = r == 0;
        if (!ok) {
            fd_set w;
            FD_ZERO(&w);
            FD_SET(s, &w);
            timeval tv{};
            tv.tv_sec = timeoutSeconds;
            ok = select(static_cast<int>(s) + 1, nullptr, &w, nullptr, &tv) == 1;
            int err = 0;
            socklen_t len = sizeof err;
            ok = ok && getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len) == 0 && err == 0;
        }
        if (!ok) {
            closeSocket(s);
            s = kBadSocket;
        }
    }
    freeaddrinfo(found);
    if (s == kBadSocket) return nullptr;
    return std::make_unique<TcpLink>(s);
}

}  // namespace sov::net
