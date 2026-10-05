// A client: applies the host's ordered stream to its own core, reports its state hash at each
// world turn, and reloads the host's save when the two drift apart.
#include "sovereign/serialize.h"
#include "sovereign_net/session.h"

namespace sov::net {

Client::Client(const Rules& rules, std::unique_ptr<Link> link, std::string name, PlayerId wantSeat, std::vector<std::string> mods)
    : rules_(rules), link_(std::move(link)) {
    Message m;
    m.type = MsgType::Hello;
    m.a = wantSeat;
    m.b = rules_.checksum();
    m.text = name.substr(0, kMaxName);
    m.list = std::move(mods);
    link_->send(encodeMessage(m));
}

std::vector<std::string> Client::takeNotices() {
    std::vector<std::string> out;
    out.swap(notices_);
    return out;
}

bool Client::submit(const Command& c) {
    if (!game_ || !connected() || c.player != seat_) return false;
    Message m;
    m.type = MsgType::Submit;
    m.command = c;
    return link_->send(encodeMessage(m));
}

void Client::chat(const std::string& text) {
    Message m;
    m.type = MsgType::Chat;
    m.text = text.substr(0, kMaxChat);
    if (connected() && !m.text.empty()) link_->send(encodeMessage(m));
}

void Client::apply(int32_t seq, const Command& c) {
    if (!game_ || resyncAsked_) return;
    const size_t have = game_->log().size();
    if (seq < 0 || static_cast<size_t>(seq) < have) return;  // already have it (a save was newer)
    const int turn = game_->state().turn;
    // A gap, or a command our core refuses where the host's accepted it: we have drifted.
    if (static_cast<size_t>(seq) > have || game_->submit(c) != CommandError::Ok) {
        Message r;
        r.type = MsgType::Resync;
        link_->send(encodeMessage(r));
        resyncAsked_ = true;
        return;
    }
    if (game_->state().turn != turn) {
        Message h;
        h.type = MsgType::Hash;
        h.a = game_->state().turn;
        h.b = game_->stateHash();
        link_->send(encodeMessage(h));
    }
}

void Client::handle(const Message& m) {
    switch (m.type) {
        case MsgType::Welcome: {
            seat_ = static_cast<PlayerId>(m.a);
            seats_ = m.seats;
            if (m.blob.empty()) return;  // still in the lobby
            std::string err;
            auto g = loadGame(rules_, m.blob, &err);
            if (!g) {
                notices_.push_back("The host's game could not be loaded: " + err);
                link_->close();
                return;
            }
            if (game_) {
                ++resyncs_;
                notices_.push_back("Resynchronised with the host.");
            } else {
                notices_.push_back("The game begins.");
            }
            game_ = std::move(g);
            resyncAsked_ = false;
            return;
        }
        case MsgType::Refuse:
            refusal_ = m.text;
            notices_.push_back("The host refused: " + m.text);
            link_->close();
            return;
        case MsgType::Seats: seats_ = m.seats; return;
        case MsgType::Apply: apply(m.a, m.command); return;
        case MsgType::Refused:
            lastRefused_ = static_cast<CommandError>(m.a);
            ++refusals_;
            notices_.push_back(std::string("The host refused your order: ") + commandErrorName(lastRefused_));
            return;
        case MsgType::Chat: {
            const std::string who = m.a >= 0 && static_cast<size_t>(m.a) < seats_.size() ? seats_[static_cast<size_t>(m.a)].name : "?";
            notices_.push_back(who + ": " + m.text);
            return;
        }
        default: return;
    }
}

void Client::poll() {
    std::vector<uint8_t> bytes;
    while (link_ && link_->receive(bytes)) {
        Message m;
        if (decodeMessage(bytes, m)) handle(m);
    }
}

}  // namespace sov::net
