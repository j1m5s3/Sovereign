// Online play: a lockstep session over the core's one command log (engine-and-architecture,
// Core foundations; 10-ai-ui-implementation, Multiplayer). Every machine runs the full rules
// core. The host orders commands: clients send theirs to it, it applies each to its own core
// and broadcasts what the core accepted, in order, and everyone applies the same stream.
// AI seats, and human seats whose player has dropped, are played on the host. At every world
// turn each client reports its state hash; on a mismatch the host sends its save and the client
// reloads. Plain C++17 over the core, like battle/ and diplomacy/; links are an interface
// (loopback here, TCP in tcp.h, the engine's or Steam's in Unreal).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sovereign/game.h"

namespace sov::net {

constexpr uint32_t kProtocolVersion = 1;
constexpr size_t kMaxChat = 300;
constexpr size_t kMaxName = 32;

// A reliable, ordered, message-framed pipe. Non-blocking.
class SOV_API Link {
public:
    virtual ~Link() = default;
    virtual bool send(const std::vector<uint8_t>& message) = 0;  // false: the link is closed
    virtual bool receive(std::vector<uint8_t>& message) = 0;     // false: nothing waiting
    virtual bool connected() const = 0;
    virtual void close() = 0;
};

// Where a host takes new connections. Non-blocking: null when nobody is waiting.
class SOV_API Listener {
public:
    virtual ~Listener() = default;
    virtual std::unique_ptr<Link> accept() = 0;
};

// In-process links, for tests and hot seat tools.
class SOV_API LoopbackListener : public Listener {
public:
    LoopbackListener();
    ~LoopbackListener() override;
    std::unique_ptr<Link> connect();  // the client's end; the host's end waits for accept()
    std::unique_ptr<Link> accept() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---- messages (protocol.cpp)
enum class MsgType : uint8_t {
    Hello = 1,  // client: protocol, rules checksum, name, wanted seat (a = -1: any), mods (list)
    Welcome,    // host: your seat (a), and the game as a save (blob; empty while in the lobby)
    Refuse,     // host: the join is refused (text)
    Seats,      // host: the seat table
    Submit,     // client: a command for the host to order
    Apply,      // host: command number a of the log, for every machine
    Refused,    // host: your command was refused (a = CommandError)
    Hash,       // client: the state hash (b) as world turn a began
    Resync,     // client: my game no longer matches; send me yours
    Chat,       // either: text from seat a (the host fills in the seat)
};

struct SeatInfo {
    std::string name;     // the player in it, or the civ's name for an AI seat
    bool human = false;   // a seat for a person (when nobody holds it, the AI plays it)
    bool connected = false;
};

struct Message {
    MsgType type = MsgType::Chat;
    int32_t a = 0;
    uint64_t b = 0;
    uint32_t protocol = kProtocolVersion;
    std::string text;
    std::vector<std::string> list;
    std::vector<uint8_t> blob;
    Command command;
    std::vector<SeatInfo> seats;
};
SOV_API std::vector<uint8_t> encodeMessage(const Message& m);
SOV_API bool decodeMessage(const std::vector<uint8_t>& bytes, Message& out);

// ---- the host
class SOV_API Host {
public:
    // `setup` lists every seat: human ones may be claimed by players who join; the host plays
    // `hostSeat` itself (kNoPlayer: a dedicated host that only orders and runs the AI).
    Host(const Rules& rules, GameSetup setup, Listener& listener, std::string hostName, PlayerId hostSeat = 0,
         std::vector<std::string> mods = {});
    // Accepts, reads every peer, plays AI seats (one turn per call), sends what changed. Call often.
    void poll();
    // Creates the game from the setup; seats nobody claimed are played by the AI. Every peer
    // gets the game.
    bool start(std::string* error = nullptr);
    bool started() const { return game_ != nullptr; }
    // The host's own command (its seat's), ordered like any other.
    CommandError submit(const Command& c);
    void chat(const std::string& text);

    const Game* game() const { return game_.get(); }
    const std::vector<SeatInfo>& seats() const { return seats_; }
    PlayerId seat() const { return hostSeat_; }
    // Lines for the screen: joins, leaves, chat, repaired desyncs.
    std::vector<std::string> takeNotices();
    int resyncsSent() const { return resyncs_; }
    // Whether the AI is playing this player now: an AI seat, a human seat nobody holds, or a
    // player outside the seat table (city-states).
    bool aiPlays(PlayerId seat) const;

private:
    struct Peer {
        std::unique_ptr<Link> link;
        PlayerId seat = kNoPlayer;
        std::string name;
        bool helloed = false;
    };
    void handle(Peer& peer, const Message& m);
    void welcome(Peer& peer);
    void broadcast(const Message& m);
    void broadcastSeats();
    void sendNew();      // Apply for every log entry not yet sent
    void noteTurn();     // remembers the state hash when a world turn begins
    void playAiSeats();
    void notice(const std::string& line) { notices_.push_back(line); }

    const Rules& rules_;
    GameSetup setup_;
    Listener& listener_;
    PlayerId hostSeat_;
    std::vector<std::string> mods_;
    std::unique_ptr<Game> game_;
    std::vector<Peer> peers_;
    std::vector<SeatInfo> seats_;
    std::map<int, uint64_t> turnHashes_;  // world turn -> the host's state hash as it began
    size_t sent_ = 0;                     // log entries broadcast so far
    int lastTurn_ = 0;
    int resyncs_ = 0;
    std::vector<std::string> notices_;
};

// ---- a client
class SOV_API Client {
public:
    Client(const Rules& rules, std::unique_ptr<Link> link, std::string name, PlayerId wantSeat = kNoPlayer,
           std::vector<std::string> mods = {});
    void poll();
    // Sends a command for the host to order; it takes effect when the host's Apply comes back.
    bool submit(const Command& c);
    void chat(const std::string& text);

    bool connected() const { return link_ && link_->connected(); }
    bool inGame() const { return game_ != nullptr; }
    PlayerId seat() const { return seat_; }
    const Game* game() const { return game_.get(); }
    Game* gameForTests() { return game_.get(); }  // to force a desync in tests
    const std::vector<SeatInfo>& seats() const { return seats_; }
    const std::string& refusal() const { return refusal_; }  // why the host refused the join
    CommandError lastRefused() const { return lastRefused_; }
    int refusals() const { return refusals_; }  // commands of ours the host refused so far
    int resyncs() const { return resyncs_; }
    std::vector<std::string> takeNotices();

private:
    void handle(const Message& m);
    void apply(int32_t seq, const Command& c);

    const Rules& rules_;
    std::unique_ptr<Link> link_;
    std::unique_ptr<Game> game_;
    PlayerId seat_ = kNoPlayer;
    std::vector<SeatInfo> seats_;
    std::string refusal_;
    CommandError lastRefused_ = CommandError::Ok;
    int refusals_ = 0;
    int resyncs_ = 0;
    bool resyncAsked_ = false;
    std::vector<std::string> notices_;
};

// For tests and tools: the commands the AI would issue for the current player of `game`,
// worked out on a copy (so a client can send them to the host instead of applying them).
SOV_API std::vector<Command> aiCommands(const Rules& rules, const Game& game);

}  // namespace sov::net
