// Online play: lobby and start, whole games in lockstep, refused orders, a forced desync and
// its repair, a dropped player's seat run by the AI and rejoined, chat, and TCP links.
#include <chrono>
#include <thread>

#include "helpers.h"
#include "sovereign_net/session.h"
#include "sovereign_net/tcp.h"

using namespace sov;
using namespace sov::net;
using sovtest::rules;

namespace {
GameSetup fourSeats() {
    GameSetup s;
    s.seed = 5;
    s.mapSize = "MAPSIZE_TINY";
    s.cityStates = 0;
    for (int i = 0; i < 4; ++i) s.players.push_back({rules().civs[static_cast<size_t>(i)].id, i < 3});
    return s;
}

// What a human's machine does on its turn: work out its orders (the AI stands in for the
// person) and send them to the host; settle a live battle that waits for it.
struct Bot {
    Client* client;
    size_t until = 0;   // the log size once everything we sent is in
    int refusals = 0;   // the client's refusal count when we sent it
    void send(const std::vector<Command>& cs) {
        until = client->game()->log().size() + cs.size();
        refusals = client->refusals();
        for (const Command& c : cs) client->submit(c);
    }
    void act() {
        const Game* g = client->game();
        if (!g || g->gameOver()) return;
        // Orders in flight: wait for them all to come back (or for a refusal).
        if (g->log().size() < until && client->refusals() == refusals) return;
        if (g->battlePending()) {
            if (g->state().pendingBattle.liveFor == client->seat()) send({Command::autoResolveBattle(client->seat())});
            return;
        }
        if (g->state().currentPlayer != client->seat()) return;
        send(aiCommands(rules(), *g));
    }
};

// The host's own seat, played the same way.
struct HostBot {
    Host* host;
    size_t waitingFor = static_cast<size_t>(-1);  // the log size when we last acted (never yet)
    void act() {
        const Game* g = host->game();
        if (!g || g->gameOver() || host->seat() == kNoPlayer) return;
        if (g->battlePending()) {
            if (g->state().pendingBattle.liveFor == host->seat() && g->log().size() != waitingFor) {
                waitingFor = g->log().size();
                host->submit(Command::autoResolveBattle(host->seat()));
            }
            return;
        }
        if (g->state().currentPlayer != host->seat() || g->log().size() == waitingFor) return;
        waitingFor = g->log().size();
        for (const Command& c : aiCommands(rules(), *g)) host->submit(c);
    }
};

void pump(Host& host, std::vector<Client*> clients, int rounds = 4) {
    for (int i = 0; i < rounds; ++i) {
        host.poll();
        for (Client* c : clients) c->poll();
    }
}

// Plays until the host's world turn reaches `turn`; false if it stalls.
bool playTo(Host& host, std::vector<Client*> clients, int turn, HostBot& hb, std::vector<Bot>& bots) {
    for (int guard = 0; guard < 20000; ++guard) {
        if (host.game()->state().turn >= turn || host.game()->gameOver()) return true;
        hb.act();
        for (Bot& b : bots) b.act();
        pump(host, clients, 1);
    }
    return false;
}

bool allMatch(const Host& host, const std::vector<Client*>& clients) {
    for (const Client* c : clients) {
        if (!c->game() || c->game()->stateHash() != host.game()->stateHash()) return false;
    }
    return true;
}
}  // namespace

TEST(messages_round_trip) {
    Message m;
    m.type = MsgType::Apply;
    m.a = 41;
    m.command = Command::proposeDeal(1, 2, {{DealItemKind::Gold, 1, 30, kNone}});
    m.command.text = "with love";
    m.seats = {{"Ann", true, true}, {"Rome", false, false}};
    Message back;
    REQUIRE(decodeMessage(encodeMessage(m), back));
    CHECK(back.type == MsgType::Apply && back.a == 41);
    CHECK(back.command.data == m.command.data && back.command.text == "with love");
    CHECK(back.seats.size() == 2u && back.seats[0].name == "Ann" && !back.seats[1].human);
    std::vector<uint8_t> bad = encodeMessage(m);
    bad.pop_back();
    CHECK(!decodeMessage(bad, back));
    bad = encodeMessage(m);
    bad[0] = 0;
    CHECK(!decodeMessage(bad, back));
}

TEST(players_join_claim_seats_and_get_the_same_game) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    Client bob(rules(), net.connect(), "Bob", 2);
    pump(host, {&ann, &bob});
    CHECK_EQ(ann.seat(), 1);
    CHECK_EQ(bob.seat(), 2);
    CHECK(!ann.inGame());
    CHECK(ann.seats()[2].name == "Bob");
    std::string err;
    REQUIRE(host.start(&err));
    pump(host, {&ann, &bob});
    REQUIRE(ann.inGame() && bob.inGame());
    CHECK(allMatch(host, {&ann, &bob}));
    CHECK(host.aiPlays(3));
    CHECK(!host.aiPlays(1));
    // A fourth player finds no free human seat once the game has begun.
    Client late(rules(), net.connect(), "Late");
    pump(host, {&late});
    CHECK(!late.refusal().empty());
}

TEST(a_whole_game_stays_in_lockstep) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    Client bob(rules(), net.connect(), "Bob");
    pump(host, {&ann, &bob});
    REQUIRE(host.start());
    pump(host, {&ann, &bob});
    HostBot hb{&host};
    std::vector<Bot> bots = {{&ann}, {&bob}};
    REQUIRE(playTo(host, {&ann, &bob}, 40, hb, bots));
    pump(host, {&ann, &bob});
    CHECK(allMatch(host, {&ann, &bob}));
    CHECK_EQ(host.resyncsSent(), 0);
    CHECK(host.game()->log().size() > 300u);
    CHECK_EQ(ann.game()->log().size(), host.game()->log().size());
}

TEST(city_states_are_played_by_the_host) {
    LoopbackListener net;
    GameSetup setup = fourSeats();
    setup.cityStates = 3;
    Host host(rules(), setup, net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    pump(host, {&ann});
    REQUIRE(host.start());
    pump(host, {&ann});
    REQUIRE(host.game()->state().players.size() > 4u);
    CHECK(host.aiPlays(4));
    HostBot hb{&host};
    std::vector<Bot> bots = {{&ann}};
    REQUIRE(playTo(host, {&ann}, 8, hb, bots));
    pump(host, {&ann});
    CHECK(allMatch(host, {&ann}));
}

TEST(orders_out_of_turn_are_refused_and_change_nothing) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    pump(host, {&ann});
    REQUIRE(host.start());
    pump(host, {&ann});
    const uint64_t before = host.game()->stateHash();
    REQUIRE(ann.submit(Command::endTurn(1)));  // seat 0 is to play
    pump(host, {&ann});
    CHECK(ann.lastRefused() == CommandError::NotYourTurn);
    CHECK_EQ(host.game()->stateHash(), before);
    // Nor can a client order for another seat.
    CHECK(!ann.submit(Command::endTurn(0)));
}

TEST(a_drifted_client_is_put_right) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    pump(host, {&ann});
    REQUIRE(host.start());
    pump(host, {&ann});
    // Something outside the rules changes Ann's copy (a bug, a cheat): the next world turn's
    // hash differs and the host sends its game.
    ann.gameForTests()->stateMutForTests().players[1].gold += Fixed::fromInt(500);
    HostBot hb{&host};
    std::vector<Bot> bots = {{&ann}};
    REQUIRE(playTo(host, {&ann}, 4, hb, bots));
    pump(host, {&ann}, 8);
    CHECK(host.resyncsSent() >= 1);
    CHECK(ann.resyncs() >= 1);
    CHECK(allMatch(host, {&ann}));
}

TEST(a_dropped_seat_is_played_by_the_ai_until_its_player_returns) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    auto ann = std::make_unique<Client>(rules(), net.connect(), "Ann");
    Client bob(rules(), net.connect(), "Bob");
    pump(host, {ann.get(), &bob});
    REQUIRE(host.start());
    pump(host, {ann.get(), &bob});
    HostBot hb{&host};
    std::vector<Bot> bots = {{ann.get()}, {&bob}};
    REQUIRE(playTo(host, {ann.get(), &bob}, 5, hb, bots));
    ann.reset();  // Ann's machine goes away
    pump(host, {&bob});
    CHECK(host.aiPlays(1));
    std::vector<Bot> bobOnly = {{&bob}};
    REQUIRE(playTo(host, {&bob}, 10, hb, bobOnly));  // the AI plays Ann's turns meanwhile
    Client back(rules(), net.connect(), "Ann", 1);
    pump(host, {&back, &bob});
    REQUIRE(back.inGame());
    CHECK_EQ(back.seat(), 1);
    CHECK(!host.aiPlays(1));
    std::vector<Bot> both = {{&back}, {&bob}};
    REQUIRE(playTo(host, {&back, &bob}, 14, hb, both));
    pump(host, {&back, &bob});
    CHECK(allMatch(host, {&back, &bob}));
}

TEST(joins_need_the_same_rules_and_mods) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host", 0, {"mod_a"});
    Client plain(rules(), net.connect(), "Plain");
    Client modded(rules(), net.connect(), "Modded", kNoPlayer, {"mod_a"});
    pump(host, {&plain, &modded});
    CHECK(plain.refusal() == "a different mod list");
    CHECK_EQ(modded.seat(), 1);
}

TEST(chat_reaches_everyone) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    Client bob(rules(), net.connect(), "Bob");
    pump(host, {&ann, &bob});
    ann.takeNotices();
    bob.takeNotices();
    host.takeNotices();
    ann.chat("Peace, friends?");
    pump(host, {&ann, &bob});
    const auto heard = bob.takeNotices();
    REQUIRE(heard.size() == 1u);
    CHECK_EQ(heard[0], std::string("Ann: Peace, friends?"));
    CHECK_EQ(host.takeNotices().back(), std::string("Ann: Peace, friends?"));
}

TEST(relays_reach_the_seat_they_are_for) {
    LoopbackListener net;
    Host host(rules(), fourSeats(), net, "Host");
    Client ann(rules(), net.connect(), "Ann");
    Client bob(rules(), net.connect(), "Bob");
    pump(host, {&ann, &bob});
    ann.sendRelay(2, {1, 2, 3});  // to Bob, through the host
    ann.sendRelay(0, {9});        // to the host's own seat
    host.sendRelay(1, {7, 7});    // the host to Ann
    pump(host, {&ann, &bob});
    const auto atBob = bob.takeRelays();
    REQUIRE(atBob.size() == 1u);
    CHECK_EQ(atBob[0].first, 1);
    CHECK(atBob[0].second == std::vector<uint8_t>({1, 2, 3}));
    const auto atHost = host.takeRelays();
    REQUIRE(atHost.size() == 1u);
    CHECK(atHost[0].first == 1 && atHost[0].second == std::vector<uint8_t>({9}));
    const auto atAnn = ann.takeRelays();
    REQUIRE(atAnn.size() == 1u);
    CHECK_EQ(atAnn[0].first, 0);
}

TEST(tcp_links_carry_a_game) {
    TcpListener listener(0, "127.0.0.1");
    REQUIRE(listener.ok());
    Host host(rules(), fourSeats(), listener, "Host");
    auto link = tcpConnect("127.0.0.1", listener.port(), 5);
    REQUIRE(link);
    Client ann(rules(), std::move(link), "Ann");
    for (int i = 0; i < 200 && ann.seat() == kNoPlayer; ++i) {
        pump(host, {&ann}, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(ann.seat() == 1);
    REQUIRE(host.start());
    for (int i = 0; i < 400 && !ann.inGame(); ++i) {
        pump(host, {&ann}, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(ann.inGame());
    HostBot hb{&host};
    std::vector<Bot> bots = {{&ann}};
    for (int guard = 0; guard < 20000 && host.game()->state().turn < 6; ++guard) {
        hb.act();
        for (Bot& b : bots) b.act();
        pump(host, {&ann}, 1);
    }
    for (int i = 0; i < 200 && ann.game()->log().size() < host.game()->log().size(); ++i) {
        pump(host, {&ann}, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(host.game()->state().turn >= 6);
    CHECK(allMatch(host, {&ann}));
    CHECK(!tcpConnect("127.0.0.1", 1, 1));  // nobody listens there
}
