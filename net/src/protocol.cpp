// Message encoding (little-endian, the save format's ByteWriter), and in-process loopback links.
#include <deque>
#include <mutex>

#include "sovereign/ai.h"
#include "sovereign/serialize.h"
#include "sovereign_net/session.h"

namespace sov::net {

namespace {
constexpr uint8_t kMagic = 0x53;  // 'S'
constexpr size_t kMaxList = 64;
}  // namespace

std::vector<uint8_t> encodeMessage(const Message& m) {
    ByteWriter w;
    w.u8(kMagic);
    w.u8(static_cast<uint8_t>(m.type));
    w.u32(m.protocol);
    w.i32(m.a);
    w.u64(m.b);
    w.str(m.text);
    w.u32(static_cast<uint32_t>(m.list.size()));
    for (const std::string& s : m.list) w.str(s);
    w.bytes(m.blob);
    const bool command = m.type == MsgType::Submit || m.type == MsgType::Apply || m.type == MsgType::Refused;
    if (command) encodeCommand(w, m.command);
    w.u32(static_cast<uint32_t>(m.seats.size()));
    for (const SeatInfo& s : m.seats) {
        w.str(s.name);
        w.boolean(s.human);
        w.boolean(s.connected);
    }
    return w.take();
}

bool decodeMessage(const std::vector<uint8_t>& bytes, Message& out) {
    ByteReader r(bytes);
    out = Message{};
    if (r.u8() != kMagic) return false;
    const uint8_t type = r.u8();
    if (type < static_cast<uint8_t>(MsgType::Hello) || type > static_cast<uint8_t>(MsgType::Chat)) return false;
    out.type = static_cast<MsgType>(type);
    out.protocol = r.u32();
    out.a = r.i32();
    out.b = r.u64();
    out.text = r.str();
    const uint32_t n = r.u32();
    if (n > kMaxList || !r.checkCount(n, 4)) return false;
    out.list.resize(n);
    for (std::string& s : out.list) s = r.str();
    out.blob = r.bytes();
    if (out.type == MsgType::Submit || out.type == MsgType::Apply || out.type == MsgType::Refused) out.command = decodeCommand(r);
    const uint32_t ns = r.u32();
    if (ns > kMaxList || !r.checkCount(ns, 6)) return false;
    out.seats.resize(ns);
    for (SeatInfo& s : out.seats) {
        s.name = r.str();
        s.human = r.boolean();
        s.connected = r.boolean();
    }
    return r.ok() && r.atEnd();
}

// ---------------------------------------------------------------- loopback

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

// ---------------------------------------------------------------- AI on a copy

std::vector<Command> aiCommands(const Rules& rules, const Game& game) {
    std::string err;
    auto copy = loadGame(rules, saveGame(game), &err);
    if (!copy) return {};
    const size_t before = copy->log().size();
    ai::playTurn(*copy);
    return std::vector<Command>(copy->log().begin() + static_cast<std::ptrdiff_t>(before), copy->log().end());
}

}  // namespace sov::net
