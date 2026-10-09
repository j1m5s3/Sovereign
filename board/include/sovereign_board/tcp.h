// TCP links for the challenge board: non-blocking sockets, each message framed by its
// 4-byte length. A separate library (sovereign_board_tcp) so engine builds can bring their own.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sovereign_board/board.h"

namespace sov::board {

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

// One-shot calls over TCP: the API a game can use to submit a save or read a week.
SubmitReply submitSave(const std::string& host, int port, int32_t week, const std::string& name,
                       const std::vector<uint8_t>& save, int timeoutSeconds = 120);
std::vector<Entry> fetchRanking(const std::string& host, int port, int32_t week, int timeoutSeconds = 10);

}  // namespace sov::board
