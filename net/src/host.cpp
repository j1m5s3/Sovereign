// The host: orders every command, plays the AI seats, repairs desyncs. It is the authority on
// order only; every machine's core still judges each command.
#include <algorithm>

#include "sovereign/ai.h"
#include "sovereign/serialize.h"
#include "sovereign_net/session.h"

namespace sov::net {

namespace {
size_t at(PlayerId p) { return static_cast<size_t>(p); }

std::string clip(const std::string& s, size_t n) {
    std::string out;
    for (char c : s) {
        if (out.size() >= n) break;
        if (static_cast<unsigned char>(c) >= 0x20) out += c;
    }
    return out;
}
}  // namespace

Host::Host(const Rules& rules, GameSetup setup, Listener& listener, std::string hostName, PlayerId hostSeat, std::vector<std::string> mods)
    : rules_(rules), setup_(std::move(setup)), listener_(listener), hostSeat_(hostSeat), mods_(std::move(mods)) {
    for (size_t i = 0; i < setup_.players.size(); ++i) {
        SeatInfo s;
        s.human = setup_.players[i].human;
        const TypeIndex civ = rules_.civ(setup_.players[i].civ);
        s.name = civ == kNone ? setup_.players[i].civ : rules_.civs[static_cast<size_t>(civ)].name;
        if (static_cast<PlayerId>(i) == hostSeat_) {
            s.human = true;
            s.connected = true;
            s.name = clip(hostName, kMaxName);
        }
        seats_.push_back(s);
    }
}

bool Host::aiPlays(PlayerId seat) const {
    if (seat < 0) return false;
    if (at(seat) >= seats_.size()) return true;  // city-states and others outside the seat table
    return !seats_[at(seat)].human || !seats_[at(seat)].connected;
}

std::vector<std::string> Host::takeNotices() {
    std::vector<std::string> out;
    out.swap(notices_);
    return out;
}

void Host::broadcast(const Message& m) {
    const std::vector<uint8_t> bytes = encodeMessage(m);
    for (Peer& p : peers_) {
        if (p.helloed && p.link->connected()) p.link->send(bytes);
    }
}

void Host::broadcastSeats() {
    Message m;
    m.type = MsgType::Seats;
    m.seats = seats_;
    broadcast(m);
}

void Host::welcome(Peer& peer) {
    Message m;
    m.type = MsgType::Welcome;
    m.a = peer.seat;
    if (game_) m.blob = saveGame(*game_);
    m.seats = seats_;
    peer.link->send(encodeMessage(m));
}

GameSetup Host::lobbySetup() const {
    GameSetup s = setup_;
    // Seats nobody claimed are the AI's for good.
    for (size_t i = 0; i < s.players.size(); ++i) s.players[i].human = seats_[i].human && seats_[i].connected;
    return s;
}

bool Host::start(std::string* error) {
    if (game_) return true;
    const GameSetup s = lobbySetup();
    std::string err;
    auto g = Game::create(rules_, s, &err);
    if (!g) {
        if (error) *error = err;
        return false;
    }
    setup_ = s;
    return start(std::move(g));
}

bool Host::start(std::unique_ptr<Game> game) {
    if (game_ || !game) return game_ != nullptr;
    game_ = std::move(game);
    for (size_t i = 0; i < seats_.size() && i < game_->state().players.size(); ++i) seats_[i].human = game_->state().players[i].human;
    sent_ = game_->log().size();
    lastTurn_ = game_->state().turn;
    turnHashes_[lastTurn_] = game_->stateHash();
    for (Peer& p : peers_) {
        if (p.helloed && p.link->connected()) welcome(p);
    }
    notice("The game begins.");
    return true;
}

void Host::noteTurn() {
    if (!game_ || game_->state().turn == lastTurn_) return;
    lastTurn_ = game_->state().turn;
    turnHashes_[lastTurn_] = game_->stateHash();
    while (turnHashes_.size() > 16) turnHashes_.erase(turnHashes_.begin());
}

void Host::sendNew() {
    if (!game_) return;
    const auto& log = game_->log();
    for (; sent_ < log.size(); ++sent_) {
        Message m;
        m.type = MsgType::Apply;
        m.a = static_cast<int32_t>(sent_);
        m.command = log[sent_];
        broadcast(m);
    }
}

CommandError Host::submit(const Command& c) {
    if (!game_ || c.player != hostSeat_) return CommandError::BadPlayer;
    const CommandError e = game_->submit(c);
    noteTurn();
    sendNew();
    return e;
}

void Host::chat(const std::string& text) {
    Message m;
    m.type = MsgType::Chat;
    m.a = hostSeat_;
    m.text = clip(text, kMaxChat);
    if (m.text.empty()) return;
    broadcast(m);
    notice((hostSeat_ >= 0 ? seats_[at(hostSeat_)].name : std::string("Host")) + ": " + m.text);
}

void Host::handle(Peer& peer, const Message& m) {
    if (!peer.helloed) {
        if (m.type != MsgType::Hello) return;
        auto refuse = [&](const std::string& why) {
            Message r;
            r.type = MsgType::Refuse;
            r.text = why;
            peer.link->send(encodeMessage(r));
            peer.link->close();
        };
        if (m.protocol != kProtocolVersion) return refuse("a different version of Sovereign");
        if (m.b != rules_.checksum()) return refuse("different rules data");
        if (m.list != mods_) return refuse("a different mod list");
        // A human seat nobody holds: the one asked for, or the first free.
        PlayerId seat = kNoPlayer;
        for (size_t i = 0; i < seats_.size(); ++i) {
            const bool free = seats_[i].human && !seats_[i].connected && static_cast<PlayerId>(i) != hostSeat_;
            const bool wanted = m.a < 0 || m.a == static_cast<int32_t>(i);
            // Before the start any seat may become a human seat.
            const bool open = !game_ && !seats_[i].connected && static_cast<PlayerId>(i) != hostSeat_;
            if ((free || open) && wanted) {
                seat = static_cast<PlayerId>(i);
                break;
            }
        }
        if (seat == kNoPlayer) return refuse("no free seat");
        peer.helloed = true;
        peer.seat = seat;
        peer.name = clip(m.text, kMaxName);
        if (peer.name.empty()) peer.name = "Player " + std::to_string(seat + 1);
        seats_[at(seat)].human = true;
        seats_[at(seat)].connected = true;
        seats_[at(seat)].name = peer.name;
        welcome(peer);
        broadcastSeats();
        notice(peer.name + (game_ ? " rejoins." : " joins."));
        return;
    }
    switch (m.type) {
        case MsgType::Submit: {
            if (!game_) return;
            CommandError e = CommandError::BadPlayer;
            if (m.command.player == peer.seat) e = game_->submit(m.command);
            if (e != CommandError::Ok) {
                Message r;
                r.type = MsgType::Refused;
                r.a = static_cast<int32_t>(e);
                r.command = m.command;
                peer.link->send(encodeMessage(r));
                return;
            }
            noteTurn();
            sendNew();
            return;
        }
        case MsgType::Hash: {
            const auto it = turnHashes_.find(m.a);
            if (it == turnHashes_.end() || it->second == m.b) return;
            ++resyncs_;
            notice(peer.name + "'s game drifted from the host's; resending it.");
            welcome(peer);
            return;
        }
        case MsgType::Resync:
            ++resyncs_;
            welcome(peer);
            return;
        case MsgType::Relay: {
            // Passed on to the seat it is for (or kept, when that is the host's own).
            const PlayerId to = static_cast<PlayerId>(m.a);
            if (to == hostSeat_) {
                relays_.push_back({peer.seat, m.blob});
                return;
            }
            Message r;
            r.type = MsgType::Relay;
            r.a = peer.seat;
            r.blob = m.blob;
            for (Peer& p : peers_) {
                if (p.seat == to && p.link->connected()) p.link->send(encodeMessage(r));
            }
            return;
        }
        case MsgType::Chat: {
            Message c;
            c.type = MsgType::Chat;
            c.a = peer.seat;
            c.text = clip(m.text, kMaxChat);
            if (c.text.empty()) return;
            broadcast(c);
            notice(peer.name + ": " + c.text);
            return;
        }
        default: return;
    }
}

void Host::sendRelay(PlayerId to, const std::vector<uint8_t>& blob) {
    Message r;
    r.type = MsgType::Relay;
    r.a = hostSeat_;
    r.blob = blob;
    for (Peer& p : peers_) {
        if (p.seat == to && p.link->connected()) p.link->send(encodeMessage(r));
    }
}

std::vector<std::pair<PlayerId, std::vector<uint8_t>>> Host::takeRelays() {
    std::vector<std::pair<PlayerId, std::vector<uint8_t>>> out;
    out.swap(relays_);
    return out;
}

void Host::playAiSeats() {
    if (!game_ || game_->gameOver()) return;
    // A live battle for a seat nobody can play is settled the Civ way, and so is one its player
    // leaves waiting past the timeout (the host settles it in that player's name, as they could).
    if (game_->battlePending()) {
        const PlayerId live = game_->state().pendingBattle.liveFor;
        const auto now = std::chrono::steady_clock::now();
        if (battleSeenAt_ != game_->log().size()) {
            battleSeenAt_ = game_->log().size();
            battleSince_ = now;
        }
        const bool late = liveBattleTimeout_ > 0 && now - battleSince_ > std::chrono::seconds(liveBattleTimeout_);
        if (late && live != kNoPlayer) {
            notice("The live battle took too long; it is settled by the numbers.");
            game_->submit(Command::autoResolveBattle(live));
            noteTurn();
            sendNew();
            return;
        }
        if (live == kNoPlayer || aiPlays(live)) {
            game_->submit(Command::autoResolveBattle(live == kNoPlayer ? game_->state().currentPlayer : live));
            noteTurn();
            sendNew();
        }
        return;
    }
    const PlayerId current = game_->state().currentPlayer;
    if (!aiPlays(current)) return;
    ai::playTurn(*game_);
    noteTurn();
    sendNew();
}

void Host::poll() {
    while (auto link = listener_.accept()) {
        Peer p;
        p.link = std::move(link);
        peers_.push_back(std::move(p));
    }
    for (Peer& p : peers_) {
        std::vector<uint8_t> bytes;
        while (p.link->connected() && p.link->receive(bytes)) {
            Message m;
            if (decodeMessage(bytes, m)) handle(p, m);
        }
    }
    // Dropped players: their seats go to the AI until they rejoin.
    for (Peer& p : peers_) {
        if (p.link->connected() || p.seat == kNoPlayer) continue;
        seats_[at(p.seat)].connected = false;
        notice(p.name + " left; the AI plays " + (game_ ? "their seat until they return." : "nothing yet."));
        p.seat = kNoPlayer;
        broadcastSeats();
    }
    peers_.erase(std::remove_if(peers_.begin(), peers_.end(), [](const Peer& p) { return !p.link->connected(); }), peers_.end());
    playAiSeats();
}

}  // namespace sov::net
