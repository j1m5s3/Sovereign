// Message encoding (little-endian, the save format's ByteWriter), loopback links, and the
// request-response server and client.
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "sovereign/serialize.h"
#include "sovereign_board/board.h"

namespace sov::board {
namespace {
constexpr uint8_t kMagic = 0x42;  // 'B'
constexpr uint32_t kMaxEntries = 4096;
}  // namespace

std::vector<uint8_t> encodeMessage(const Message& m) {
    ByteWriter w;
    w.u8(kMagic);
    w.u8(static_cast<uint8_t>(m.type));
    w.u32(m.protocol);
    w.i32(m.week);
    w.i32(m.rank);
    w.str(m.name);
    w.str(m.error);
    w.boolean(m.result.valid);
    w.boolean(m.result.goalMet);
    w.i32(m.result.turn);
    w.i32(m.result.score);
    w.str(m.result.error);
    w.bytes(m.save);
    w.u32(static_cast<uint32_t>(m.entries.size()));
    for (const Entry& e : m.entries) {
        w.str(e.name);
        w.boolean(e.goalMet);
        w.i32(e.turn);
        w.i32(e.score);
    }
    return w.take();
}

bool decodeMessage(const std::vector<uint8_t>& bytes, Message& out) {
    ByteReader r(bytes);
    out = Message{};
    if (r.u8() != kMagic) return false;
    const uint8_t type = r.u8();
    if (type < static_cast<uint8_t>(MsgType::Submit) || type > static_cast<uint8_t>(MsgType::Rankings)) return false;
    out.type = static_cast<MsgType>(type);
    out.protocol = r.u32();
    out.week = r.i32();
    out.rank = r.i32();
    out.name = r.str();
    out.error = r.str();
    out.result.valid = r.boolean();
    out.result.goalMet = r.boolean();
    out.result.turn = r.i32();
    out.result.score = r.i32();
    out.result.error = r.str();
    out.save = r.bytes();
    const uint32_t n = r.u32();
    if (n > kMaxEntries || !r.checkCount(n, 6)) return false;
    out.entries.resize(n);
    for (Entry& e : out.entries) {
        e.name = r.str();
        e.goalMet = r.boolean();
        e.turn = r.i32();
        e.score = r.i32();
    }
    return r.ok() && r.atEnd();
}

namespace {
struct Pipe {
    std::mutex lock;
    std::deque<std::vector<uint8_t>> toHost, toClient;
    bool closed = false;
};

class LoopbackLink : public Link {
public:
    LoopbackLink(std::shared_ptr<Pipe> pipe, bool hostSide) : pipe_(std::move(pipe)), host_(hostSide) {}
    ~LoopbackLink() override { close(); }
    bool send(const std::vector<uint8_t>& m) override {
        std::lock_guard<std::mutex> g(pipe_->lock);
        if (pipe_->closed) return false;
        (host_ ? pipe_->toClient : pipe_->toHost).push_back(m);
        return true;
    }
    bool receive(std::vector<uint8_t>& m) override {
        std::lock_guard<std::mutex> g(pipe_->lock);
        auto& q = host_ ? pipe_->toHost : pipe_->toClient;
        if (q.empty()) return false;
        m = std::move(q.front());
        q.pop_front();
        return true;
    }
    bool connected() const override {
        std::lock_guard<std::mutex> g(pipe_->lock);
        return !pipe_->closed;
    }
    void close() override {
        std::lock_guard<std::mutex> g(pipe_->lock);
        pipe_->closed = true;
    }

private:
    std::shared_ptr<Pipe> pipe_;
    bool host_;
};
}  // namespace

struct LoopbackListener::Impl {
    std::mutex lock;
    std::deque<std::shared_ptr<Pipe>> waiting;
};

LoopbackListener::LoopbackListener() : impl_(std::make_unique<Impl>()) {}
LoopbackListener::~LoopbackListener() = default;

std::unique_ptr<Link> LoopbackListener::connect() {
    auto pipe = std::make_shared<Pipe>();
    std::lock_guard<std::mutex> g(impl_->lock);
    impl_->waiting.push_back(pipe);
    return std::make_unique<LoopbackLink>(pipe, false);
}

std::unique_ptr<Link> LoopbackListener::accept() {
    std::lock_guard<std::mutex> g(impl_->lock);
    if (impl_->waiting.empty()) return nullptr;
    auto pipe = impl_->waiting.front();
    impl_->waiting.pop_front();
    return std::make_unique<LoopbackLink>(pipe, true);
}

Server::Server(Board& board, Listener& listener) : board_(board), listener_(listener) {}

void Server::handle(Link& link, const std::vector<uint8_t>& bytes) {
    Message req, reply;
    if (!decodeMessage(bytes, req) || req.protocol != kProtocolVersion) {
        reply.type = MsgType::Rejected;
        reply.error = "bad request";
        link.send(encodeMessage(reply));
        return;
    }
    if (req.type == MsgType::Submit) {
        const SubmitReply r = board_.submit(req.week, req.name, req.save);
        reply.week = req.week;
        reply.name = req.name;
        reply.result = r.result;
        reply.rank = r.rank;
        if (r.accepted) {
            reply.type = MsgType::Accepted;
        } else {
            reply.type = MsgType::Rejected;
            reply.error = r.error;
        }
        link.send(encodeMessage(reply));
        return;
    }
    if (req.type == MsgType::List) {
        reply.type = MsgType::Rankings;
        reply.week = req.week;
        reply.entries = board_.ranking(req.week);
        link.send(encodeMessage(reply));
        return;
    }
    reply.type = MsgType::Rejected;
    reply.error = "unknown request";
    link.send(encodeMessage(reply));
}

void Server::poll() {
    while (std::unique_ptr<Link> c = listener_.accept()) conns_.push_back(std::move(c));
    for (size_t i = 0; i < conns_.size();) {
        std::vector<uint8_t> bytes;
        if (conns_[i]->receive(bytes)) {
            handle(*conns_[i], bytes);
            conns_[i]->close();
            conns_.erase(conns_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (!conns_[i]->connected()) {
            conns_.erase(conns_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

Client::Client(std::unique_ptr<Link> link, int timeoutSeconds) : link_(std::move(link)), timeoutSeconds_(timeoutSeconds) {}

bool Client::connected() const { return link_ && link_->connected(); }

bool Client::exchange(const Message& req, Message& reply) {
    if (!link_ || !link_->send(encodeMessage(req))) {
        reply.error = "not connected";
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds_);
    std::vector<uint8_t> bytes;
    while (std::chrono::steady_clock::now() < deadline) {
        if (link_->receive(bytes)) return decodeMessage(bytes, reply);
        if (!link_->connected()) {
            reply.error = "disconnected";
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    reply.error = "timeout";
    return false;
}

SubmitReply Client::submit(int32_t week, const std::string& name, const std::vector<uint8_t>& save) {
    Message req;
    req.type = MsgType::Submit;
    req.week = week;
    req.name = name;
    req.save = save;
    Message reply;
    SubmitReply r;
    if (!exchange(req, reply)) {
        r.error = reply.error.empty() ? "no reply" : reply.error;
        return r;
    }
    r.result = reply.result;
    r.rank = reply.rank;
    if (reply.type == MsgType::Accepted) {
        r.accepted = true;
        return r;
    }
    r.error = reply.error.empty() ? "rejected" : reply.error;
    return r;
}

std::vector<Entry> Client::ranking(int32_t week) {
    Message req;
    req.type = MsgType::List;
    req.week = week;
    Message reply;
    if (!exchange(req, reply) || reply.type != MsgType::Rankings) return {};
    return reply.entries;
}

}  // namespace sov::board
