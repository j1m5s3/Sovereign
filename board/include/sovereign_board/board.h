// The weekly challenge's server board (player-retention §3): a submitted save is checked with
// checkChallenge, and valid games rank by the goal met, then fewer turns, then a higher score.
// Plain C++17 over the rules core, like net/. Links are an interface (loopback here, TCP in tcp.h);
// engine builds can bring their own transport. The socket code is a separate library so it stays
// out of Unreal.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sovereign/api.h"
#include "sovereign/challenge.h"

namespace sov {
class Rules;
}

namespace sov::board {

constexpr uint32_t kProtocolVersion = 1;
constexpr size_t kMaxName = 32;

struct Entry {
    std::string name;
    bool goalMet = false;
    int32_t turn = 0;
    int32_t score = 0;
};

// True when `a` ranks above `b` (goal met, then fewer turns, then a higher score, then the name).
inline bool better(const Entry& a, const Entry& b) {
    if (a.goalMet != b.goalMet) return a.goalMet;
    if (a.turn != b.turn) return a.turn < b.turn;
    if (a.score != b.score) return a.score > b.score;
    return a.name < b.name;
}

struct SubmitReply {
    bool accepted = false;
    std::string error;
    int32_t rank = 0;  // 1-based when accepted
    ChallengeResult result;
};

// One week's ranked list on disk (`<dataDir>/week-<n>.txt`). The same name keeps its best result.
class SOV_API Board {
public:
    Board(const Rules& rules, std::string dataDir);
    SubmitReply submit(int32_t week, std::string name, const std::vector<uint8_t>& save);
    std::vector<Entry> ranking(int32_t week) const;

private:
    const Rules& rules_;
    std::string dataDir_;
};

// A reliable, ordered, message-framed pipe. Non-blocking.
class SOV_API Link {
public:
    virtual ~Link() = default;
    virtual bool send(const std::vector<uint8_t>& message) = 0;
    virtual bool receive(std::vector<uint8_t>& message) = 0;
    virtual bool connected() const = 0;
    virtual void close() = 0;
};

// Where a server takes new connections. Non-blocking: null when nobody is waiting.
class SOV_API Listener {
public:
    virtual ~Listener() = default;
    virtual std::unique_ptr<Link> accept() = 0;
};

// In-process links, for tests.
class SOV_API LoopbackListener : public Listener {
public:
    LoopbackListener();
    ~LoopbackListener() override;
    std::unique_ptr<Link> connect();
    std::unique_ptr<Link> accept() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

enum class MsgType : uint8_t { Submit = 1, List, Accepted, Rejected, Rankings };

struct Message {
    MsgType type = MsgType::List;
    uint32_t protocol = kProtocolVersion;
    int32_t week = 0;
    int32_t rank = 0;
    std::string name;
    std::string error;
    std::vector<uint8_t> save;
    ChallengeResult result;
    std::vector<Entry> entries;
};
SOV_API std::vector<uint8_t> encodeMessage(const Message& m);
SOV_API bool decodeMessage(const std::vector<uint8_t>& bytes, Message& out);

// Accepts Submit and List, one request per connection.
class SOV_API Server {
public:
    Server(Board& board, Listener& listener);
    void poll();

private:
    void handle(Link& link, const std::vector<uint8_t>& bytes);
    Board& board_;
    Listener& listener_;
    std::vector<std::unique_ptr<Link>> conns_;
};

// Blocking client API the game can call later (one request per connection).
class SOV_API Client {
public:
    explicit Client(std::unique_ptr<Link> link, int timeoutSeconds = 120);
    SubmitReply submit(int32_t week, const std::string& name, const std::vector<uint8_t>& save);
    std::vector<Entry> ranking(int32_t week);
    bool connected() const;

private:
    bool exchange(const Message& req, Message& reply);
    std::unique_ptr<Link> link_;
    int timeoutSeconds_ = 120;
};

}  // namespace sov::board
