// sovnet: a networked game between processes or machines, every human seat played by the AI
// standing in for its person. It shows the lockstep working over TCP.
//
//   sovnet host --rules data/rules [--port 7777] [--players 4] [--humans 2] [--turns 30] [--seed 5]
//   sovnet join --rules data/rules [--host 127.0.0.1] [--port 7777] [--name Ann] [--turns 30]
//
// The host waits for humans-1 players, starts, and both sides play until --turns. Each prints
// its state hash at the end: they match when the lockstep held.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "sovereign_net/session.h"
#include "sovereign_net/tcp.h"

using namespace sov;
using namespace sov::net;

namespace {
void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void print(const std::vector<std::string>& lines) {
    for (const std::string& l : lines) std::printf("  %s\n", l.c_str());
}

// Sends this seat's turn (the AI standing in for its person). `until` is the log size once
// everything sent is back; nothing more is sent before then unless a refusal arrives.
template <typename Submit>
void playSeat(const Rules& rules, const Game& g, PlayerId seat, size_t& until, int& refusalsSeen, int refusals, Submit submit) {
    if (g.gameOver()) return;
    if (g.log().size() < until && refusals == refusalsSeen) return;
    std::vector<Command> cs;
    if (g.battlePending()) {
        if (g.state().pendingBattle.liveFor == seat) cs.push_back(Command::autoResolveBattle(seat));
    } else if (g.state().currentPlayer == seat) {
        cs = aiCommands(rules, g);
    }
    if (cs.empty()) return;
    until = g.log().size() + cs.size();
    refusalsSeen = refusals;
    for (const Command& c : cs) submit(c);
}
}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // progress shows at once, even into a file
    if (argc < 2) {
        std::fprintf(stderr, "usage: sovnet host|join [options]\n");
        return 2;
    }
    const std::string mode = argv[1];
    std::string rulesDir = "data/rules", host = "127.0.0.1", name = "Player";
    int port = 7777, players = 4, humans = 2, turns = 30;
    uint64_t seed = 5;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string a = argv[i], v = argv[i + 1];
        if (a == "--rules") rulesDir = v;
        else if (a == "--host") host = v;
        else if (a == "--name") name = v;
        else if (a == "--port") port = std::atoi(v.c_str());
        else if (a == "--players") players = std::atoi(v.c_str());
        else if (a == "--humans") humans = std::atoi(v.c_str());
        else if (a == "--turns") turns = std::atoi(v.c_str());
        else if (a == "--seed") seed = std::strtoull(v.c_str(), nullptr, 10);
        else {
            std::fprintf(stderr, "unknown option %s\n", a.c_str());
            return 2;
        }
    }
    Rules rules;
    std::string err;
    if (!rules.load({rulesDir}, &err)) {
        std::fprintf(stderr, "cannot load rules: %s\n", err.c_str());
        return 1;
    }
    if (mode == "host") {
        TcpListener listener(port);
        if (!listener.ok()) {
            std::fprintf(stderr, "cannot listen on port %d\n", port);
            return 1;
        }
        GameSetup setup;
        setup.seed = seed;
        setup.mapSize = "MAPSIZE_TINY";
        for (int i = 0; i < players; ++i) setup.players.push_back({rules.civs[static_cast<size_t>(i) % rules.civs.size()].id, i < humans});
        Host h(rules, setup, listener, "Host");
        std::printf("Hosting on port %d; waiting for %d player(s).\n", listener.port(), humans - 1);
        auto joined = [&]() {
            int n = 0;
            for (size_t i = 1; i < h.seats().size(); ++i) n += h.seats()[i].connected ? 1 : 0;
            return n;
        };
        while (joined() < humans - 1) {
            h.poll();
            print(h.takeNotices());
            sleepMs(10);
        }
        if (!h.start(&err)) {
            std::fprintf(stderr, "cannot start: %s\n", err.c_str());
            return 1;
        }
        size_t until = 0;
        int seen = 0, shown = 0;
        while (h.game()->state().turn < turns && !h.game()->gameOver()) {
            playSeat(rules, *h.game(), h.seat(), until, seen, 0, [&](const Command& c) { h.submit(c); });
            h.poll();
            print(h.takeNotices());
            if (h.game()->state().turn != shown && h.game()->state().turn % 10 == 0) {
                shown = h.game()->state().turn;
                std::printf("turn %d\n", shown);
            }
            sleepMs(1);
        }
        // Give the others time to take in the last commands.
        for (int i = 0; i < 300; ++i) {
            h.poll();
            sleepMs(10);
        }
        std::printf("turn %d, %zu commands, state hash %016llx, resyncs sent %d\n", h.game()->state().turn, h.game()->log().size(),
                    static_cast<unsigned long long>(h.game()->stateHash()), h.resyncsSent());
        return 0;
    }
    if (mode == "join") {
        auto link = tcpConnect(host, port, 10);
        if (!link) {
            std::fprintf(stderr, "nobody answered at %s:%d\n", host.c_str(), port);
            return 1;
        }
        Client c(rules, std::move(link), name);
        size_t until = 0;
        int seen = 0, quiet = 0, shown = -1;
        while (c.connected() && quiet < 300) {
            c.poll();
            print(c.takeNotices());
            if (c.game() && shown < 0) std::printf("In the game as seat %d.\n", c.seat());
            if (c.game() && c.game()->state().turn != shown && (shown < 0 || c.game()->state().turn % 10 == 0)) {
                shown = c.game()->state().turn;
                std::printf("turn %d\n", shown);
            }
            if (c.game()) {
                const size_t before = c.game()->log().size();
                playSeat(rules, *c.game(), c.seat(), until, seen, c.refusals(), [&](const Command& cmd) { c.submit(cmd); });
                // After the last turn, stop once the stream has gone quiet.
                const bool done = c.game()->state().turn >= turns || c.game()->gameOver();
                quiet = done && c.game()->log().size() == before ? quiet + 1 : 0;
            }
            sleepMs(10);
        }
        if (!c.refusal().empty()) return 1;
        if (!c.game()) {
            std::fprintf(stderr, "the game never started\n");
            return 1;
        }
        std::printf("seat %d, turn %d, %zu commands, state hash %016llx, resyncs %d\n", c.seat(), c.game()->state().turn,
                    c.game()->log().size(), static_cast<unsigned long long>(c.game()->stateHash()), c.resyncs());
        return 0;
    }
    std::fprintf(stderr, "usage: sovnet host|join [options]\n");
    return 2;
}
