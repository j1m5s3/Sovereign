// SocketTransport: blocking sockets to a local model server (Winsock on Windows, BSD sockets
// elsewhere). One request per connection ("Connection: close").
#include "sovereign_diplomacy/http.h"

#include <cstdlib>
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
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kBadSocket = -1;
#endif

namespace sov::diplomacy {

namespace {
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
#else
bool startSockets() { return true; }
void closeSocket(SocketHandle s) { close(s); }
#endif

void setTimeout(SocketHandle s, int seconds) {
#ifdef _WIN32
    const DWORD ms = static_cast<DWORD>(seconds) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof ms);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof ms);
#else
    timeval tv{};
    tv.tv_sec = seconds;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#endif
}

std::string lowerCopy(std::string s) {
    for (char& c : s) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return s;
}
}  // namespace

SocketTransport::SocketTransport(std::string host, int port, int timeoutSeconds)
    : host_(std::move(host)), port_(port), timeout_(timeoutSeconds),
      local_(host_ == "127.0.0.1" || host_ == "localhost" || host_ == "::1") {}

bool SocketTransport::request(const std::string& head, const std::string& body, std::string& response) {
    if (!local_ || !startSockets()) return false;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &found) != 0 || !found) return false;
    SocketHandle s = kBadSocket;
    for (addrinfo* a = found; a; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == kBadSocket) continue;
        setTimeout(s, timeout_);
        if (connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen)) == 0) break;
        closeSocket(s);
        s = kBadSocket;
    }
    freeaddrinfo(found);
    if (s == kBadSocket) return false;
    const std::string out = head + body;
    size_t sent = 0;
    while (sent < out.size()) {
        const int n = static_cast<int>(send(s, out.data() + sent, static_cast<int>(out.size() - sent), 0));
        if (n <= 0) {
            closeSocket(s);
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    std::string raw;
    char buf[4096];
    for (;;) {
        const int n = static_cast<int>(recv(s, buf, static_cast<int>(sizeof buf), 0));
        if (n <= 0) break;
        raw.append(buf, static_cast<size_t>(n));
        if (raw.size() > (8u << 20)) break;  // no reply is this long
    }
    closeSocket(s);
    return parseHttpResponse(raw, response);
}

bool SocketTransport::postJson(const std::string& path, const std::string& body, std::string& response) {
    const std::string head = "POST " + path + " HTTP/1.1\r\nHost: " + host_ + ":" + std::to_string(port_) +
                             "\r\nContent-Type: application/json\r\nAccept: application/json\r\nConnection: close\r\nContent-Length: " +
                             std::to_string(body.size()) + "\r\n\r\n";
    return request(head, body, response);
}

bool SocketTransport::healthy() {
    std::string response;
    return request("GET /health HTTP/1.1\r\nHost: " + host_ + "\r\nConnection: close\r\n\r\n", "", response);
}

bool parseHttpResponse(const std::string& raw, std::string& body) {
    const size_t headEnd = raw.find("\r\n\r\n");
    if (raw.compare(0, 5, "HTTP/") != 0 || headEnd == std::string::npos) return false;
    const size_t sp = raw.find(' ');
    const int status = sp == std::string::npos ? 0 : std::atoi(raw.c_str() + sp + 1);
    const std::string head = lowerCopy(raw.substr(0, headEnd));
    std::string rest = raw.substr(headEnd + 4);
    if (head.find("transfer-encoding: chunked") != std::string::npos) {
        std::string out;
        size_t at = 0;
        for (;;) {
            const size_t eol = rest.find("\r\n", at);
            if (eol == std::string::npos) return false;
            const size_t len = std::strtoul(rest.substr(at, eol - at).c_str(), nullptr, 16);
            if (len == 0) break;
            if (eol + 2 + len > rest.size()) return false;
            out.append(rest, eol + 2, len);
            at = eol + 2 + len + 2;
        }
        rest = out;
    } else {
        const size_t cl = head.find("content-length:");
        if (cl != std::string::npos) {
            const size_t len = std::strtoul(head.c_str() + cl + 15, nullptr, 10);
            if (len < rest.size()) rest.resize(len);
        }
    }
    body = rest;
    return status >= 200 && status < 300;
}

}  // namespace sov::diplomacy
