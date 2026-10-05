// TCP links for LAN and direct-IP play: non-blocking sockets, each message framed by its
// 4-byte length. A separate library (sovereign_net_tcp) so engine builds can bring their own.
#pragma once

#include <memory>
#include <string>

#include "sovereign_net/session.h"

namespace sov::net {

class TcpListener : public Listener {
public:
    // Listens on every interface (or `bindAddress`); port 0 picks a free one (see port()).
    explicit TcpListener(int port, const std::string& bindAddress = "0.0.0.0");
    ~TcpListener() override;
    bool ok() const;
    int port() const;
    std::unique_ptr<Link> accept() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Connects (blocking, with a timeout); null when nobody answered.
std::unique_ptr<Link> tcpConnect(const std::string& host, int port, int timeoutSeconds = 10);

}  // namespace sov::net
