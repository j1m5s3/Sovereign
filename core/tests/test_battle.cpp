// The live battle contract (leader doc §9): melee with a human's leader stack waits for
// a field result, which the core clamps to a band around the expected Civ result.
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
Hex hx(int x, int y) { return Hex{x, y}; }

UnitId addLeader(GameState& s, PlayerId owner, Hex pos) {
    const UnitId id = addUnit(s, "UNIT_SOVEREIGN", owner, pos);
    s.units.back().gear = {rules().gearType("GEAR_SWORD"), rules().gearType("GEAR_IRON_MAIL"), kNone};
    return id;
}

struct Field {
    std::unique_ptr<Game> game;
    UnitId leader = 0, escort = 0, enemy = 0;
};

// Player 0 (human) has its leader and a Swordsman escort at (6,6); player 1 (AI or
// human) has a Swordsman at (7,6). Both are at war; live battles are on.
Field field(bool live = true, bool enemyHuman = false) {
    Field f;
    GameState s = flatState(16, 12, 2);
    s.setup.liveBattles = live;
    s.setup.players[1].human = enemyHuman;
    addCity(s, 0, {2, 2}, true);
    addCity(s, 1, {13, 9}, true);
    f.leader = addLeader(s, 0, {6, 6});
    f.escort = addUnit(s, "UNIT_SWORDSMAN", 0, {6, 6});
    f.enemy = addUnit(s, "UNIT_SWORDSMAN", 1, {7, 6});
    for (Player& p : s.players) p.human = s.setup.players[static_cast<size_t>(p.id)].human;
    f.game = Game::fromScenario(rules(), std::move(s));
    f.game->submit(Command::declareWar(0, 1));
    return f;
}
}  // namespace

TEST(leader_stack_melee_waits_for_its_live_battle) {
    Field f = field();
    Game& g = *f.game;
    CHECK_EQ(g.liveBattleSide(*g.state().unit(f.escort), *g.state().unit(f.enemy)), 0);
    REQUIRE(g.submit(Command::attack(0, f.escort, {7, 6})) == CommandError::Ok);
    const PendingBattle& b = g.state().pendingBattle;
    REQUIRE(b.active);
    CHECK(b.attacker == f.escort && b.defender == f.enemy && b.liveFor == 0 && b.leader == f.leader);
    CHECK(b.expectedToDefender > 0 && b.expectedToAttacker > 0);
    CHECK_EQ(g.state().unit(f.enemy)->hp, 100);  // nothing has happened yet
    // The world waits: no other command, not even ending the turn.
    CHECK_EQ(g.submit(Command::endTurn(0)), CommandError::BattlePending);
    CHECK_EQ(g.submit(Command::setActivity(0, f.leader, Activity::Sleep)), CommandError::BattlePending);
    CHECK_EQ(g.submit(Command::battleResult(1, 50, 0, 0)), CommandError::BattlePending);  // not the battle's human
}

TEST(field_results_are_clamped_to_the_band) {
    Field f = field();
    Game& g = *f.game;
    REQUIRE(g.submit(Command::attack(0, f.escort, {7, 6})) == CommandError::Ok);
    const PendingBattle b = g.state().pendingBattle;
    const int band = rules().globalInt("LIVE_BATTLE_BAND_PERCENT");
    // A heroic claim (no losses, the enemy wiped out) is pulled back into the band.
    REQUIRE(g.submit(Command::battleResult(0, 1000, 0, 500)) == CommandError::Ok);
    CHECK(!g.battlePending());
    const int dealt = 100 - g.state().unit(f.enemy)->hp;
    const int taken = 100 - g.state().unit(f.escort)->hp;
    CHECK_EQ(dealt, (b.expectedToDefender * (100 + band) + 99) / 100);
    CHECK_EQ(taken, b.expectedToAttacker * (100 - band) / 100);
    CHECK_EQ(g.state().unit(f.leader)->hp, 100 - rules().globalInt("LIVE_BATTLE_LEADER_MAX_WOUND"));
    // The result is a logged command: replays reproduce it exactly.
    GameState s0 = g.state();
    (void)s0;
}

TEST(auto_resolve_uses_the_normal_roll) {
    Field f = field();
    Game& g = *f.game;
    REQUIRE(g.submit(Command::attack(0, f.escort, {7, 6})) == CommandError::Ok);
    CHECK_EQ(g.submit(Command::autoResolveBattle(0)), CommandError::Ok);
    CHECK(!g.battlePending());
    CHECK(g.state().unit(f.enemy)->hp < 100);
    CHECK_EQ(g.submit(Command::autoResolveBattle(0)), CommandError::NoBattle);
}

TEST(no_live_battles_without_the_switch_or_a_leader) {
    Field off = field(false);
    REQUIRE(off.game->submit(Command::attack(0, off.escort, {7, 6})) == CommandError::Ok);
    CHECK(!off.game->battlePending());
    CHECK(off.game->state().unit(off.enemy)->hp < 100);
    // A unit away from its leader fights the normal way.
    GameState s = flatState(16, 12, 2);
    s.setup.liveBattles = true;
    addLeader(s, 0, {2, 2});
    const UnitId lone = addUnit(s, "UNIT_SWORDSMAN", 0, {6, 6});
    const UnitId foe = addUnit(s, "UNIT_SWORDSMAN", 1, {7, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    g->submit(Command::declareWar(0, 1));
    REQUIRE(g->submit(Command::attack(0, lone, {7, 6})) == CommandError::Ok);
    CHECK(!g->battlePending());
    CHECK(g->state().unit(foe)->hp < 100);
}

TEST(an_ai_attack_pauses_for_the_human_and_resumes) {
    Field f = field();
    Game& g = *f.game;
    for (UnitId id : g.unitsNeedingOrders(0)) g.submit(Command::setActivity(0, id, Activity::Fortify));
    for (UnitId id : g.unitsNeedingOrders(0)) g.submit(Command::setActivity(0, id, Activity::Sleep));
    sovtest::endTurns(g, 1);  // also picks research and a civic
    REQUIRE(g.state().currentPlayer == 1);
    // The AI's own attack on the leader stack goes live and pauses it.
    REQUIRE(g.submit(Command::attack(1, f.enemy, {6, 6})) == CommandError::Ok);
    REQUIRE(g.battlePending());
    CHECK_EQ(g.state().pendingBattle.liveFor, 0);
    ai::playTurn(g);  // returns at once while the battle waits
    CHECK_EQ(g.state().currentPlayer, 1);
    // The human answers out of turn; then the AI finishes its turn.
    CHECK_EQ(g.submit(Command::battleResult(0, 0, 0, 0)), CommandError::Ok);
    ai::playTurn(g);
    CHECK_EQ(g.state().currentPlayer, 0);
}

TEST(live_battles_replay) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(5);
    setup.liveBattles = true;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    // Play AI turns for both seats, settling any battle with a fixed field result.
    for (int i = 0; i < 400 && g->state().turn < 60 && !g->gameOver(); ++i) {
        if (g->battlePending()) {
            g->submit(Command::battleResult(g->state().pendingBattle.liveFor, 30, 20, 5));
            continue;
        }
        ai::playTurn(*g);
    }
    auto replayed = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(replayed);
    CHECK_EQ(replayed->stateHash(), g->stateHash());
}

// ---- city assaults (leader doc §9: a city holding the leader is assaulted)

namespace {
struct Siege {
    std::unique_ptr<Game> game;
    CityId city = kNoCity;
    UnitId leader = 0, attacker = 0;
};

// Player 0 (human) keeps its leader in a walled-off city at (6,6); player 1's Swordsman stands next to it.
Siege siege(bool live = true, bool fromSea = false) {
    Siege f;
    GameState s = flatState(16, 12, 2);
    s.setup.liveBattles = live;
    s.setup.players[1].human = false;
    f.city = addCity(s, 0, {6, 6}, true, 3);
    addCity(s, 1, {13, 9}, true);
    f.leader = addLeader(s, 0, {6, 6});
    if (fromSea) s.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
    f.attacker = addUnit(s, fromSea ? "UNIT_GALLEY" : "UNIT_SWORDSMAN", 1, {7, 6});
    s.players[0].human = true;
    s.players[1].human = false;
    f.game = Game::fromScenario(rules(), std::move(s));
    f.game->submit(Command::declareWar(0, 1));
    f.game->submit(Command::setActivity(0, f.leader, Activity::Sleep));
    sovtest::endTurns(*f.game, 1);
    return f;
}
}  // namespace

TEST(assault_on_a_city_holding_the_leader_goes_live) {
    Siege f = siege();
    Game& g = *f.game;
    REQUIRE(g.state().currentPlayer == 1);
    const int hp = g.state().city(f.city)->hp;
    REQUIRE(g.submit(Command::attack(1, f.attacker, {6, 6})) == CommandError::Ok);
    const PendingBattle& b = g.state().pendingBattle;
    REQUIRE(b.active);
    CHECK(b.city == f.city && b.liveFor == 0 && b.leader == f.leader && b.defender == kNoUnit);
    CHECK_EQ(g.state().city(f.city)->hp, hp);
    // The defender answers out of turn; the city's loss is clamped to the band.
    const int band = rules().globalInt("LIVE_BATTLE_BAND_PERCENT");
    const int expected = b.expectedToDefender;
    REQUIRE(g.submit(Command::battleResult(0, 0, 999, 10)) == CommandError::Ok);
    CHECK_EQ(hp - g.state().city(f.city)->hp, expected * (100 - band) / 100);
    CHECK_EQ(g.state().unit(f.leader)->hp, 90);
}

TEST(city_assaults_auto_resolve_and_stay_civ_math_without_the_switch) {
    Siege f = siege();
    REQUIRE(f.game->submit(Command::attack(1, f.attacker, {6, 6})) == CommandError::Ok);
    const int hp = f.game->state().city(f.city)->hp;
    CHECK_EQ(f.game->submit(Command::autoResolveBattle(1)), CommandError::Ok);  // the attacker may settle it too
    CHECK(f.game->state().city(f.city)->hp < hp);
    Siege off = siege(false);
    REQUIRE(off.game->submit(Command::attack(1, off.attacker, {6, 6})) == CommandError::Ok);
    CHECK(!off.game->battlePending());
}

TEST(a_leader_killed_storming_a_city_starts_a_succession) {
    GameState s = flatState(16, 12, 2);
    addCity(s, 0, {2, 2}, true);
    const CityId target = addCity(s, 1, {7, 6}, true, 6);
    const UnitId leader = addLeader(s, 0, {6, 6});
    s.units.back().hp = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    g->submit(Command::declareWar(0, 1));
    REQUIRE(g->submit(Command::attack(0, leader, {7, 6})) == CommandError::Ok);
    CHECK(!g->leaderOf(0));
    CHECK(g->state().players[0].successionPending);
    (void)target;
}

TEST(live_battle_habits_feed_the_profile) {
    Field f = field();
    Game& g = *f.game;
    REQUIRE(g.submit(Command::attack(0, f.escort, {7, 6})) == CommandError::Ok);
    CHECK_EQ(g.submit(Command::battleResult(0, 30, 20, 0, {500, 0, 0, 1001})), CommandError::BadTarget);  // out of range
    REQUIRE(g.submit(Command::battleResult(0, 30, 20, 0, {800, 100, 0, 600})) == CommandError::Ok);
    const PlayerProfile* p = g.profile(0);
    REQUIRE(p != nullptr);
    CHECK_EQ(p->battles, 1);
    CHECK_EQ(p->battleFlank, 800);
    CHECK_EQ(p->battleLeaderFront, 600);
    // A plain result (no habits) still settles the battle and leaves the profile alone.
    Field f2 = field();
    REQUIRE(f2.game->submit(Command::attack(0, f2.escort, {7, 6})) == CommandError::Ok);
    REQUIRE(f2.game->submit(Command::battleResult(0, 30, 20, 0)) == CommandError::Ok);
    CHECK(f2.game->profile(0) == nullptr || f2.game->profile(0)->battles == 0);
}

// Open gaps review 13: naval fights with the leader auto-resolve, and a leader beaten at sea dies with its transport.
TEST(naval_fights_with_the_leader_auto_resolve) {
    GameState s = flatState(16, 12, 2);
    s.setup.liveBattles = true;
    for (Hex h : {hx(6, 6), hx(7, 6)}) s.plot(h).terrain = rules().terrain("TERRAIN_COAST");
    addCity(s, 0, {2, 2}, true);
    addCity(s, 1, {13, 9}, true);
    const UnitId leader = addLeader(s, 0, {6, 6});
    s.units.back().hp = 5;
    const UnitId galley = addUnit(s, "UNIT_GALLEY", 1, {7, 6});
    for (Player& p : s.players) p.human = s.setup.players[static_cast<size_t>(p.id)].human;
    REQUIRE(s.players[0].human);
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    REQUIRE(g->isEmbarked(*g->state().unit(leader)));
    CHECK_EQ(g->liveBattleSide(*g->state().unit(galley), *g->state().unit(leader)), kNoPlayer);
    for (UnitId id : g->unitsNeedingOrders(0)) g->submit(Command::setActivity(0, id, Activity::Sleep));
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    REQUIRE(g->submit(Command::attack(1, galley, {6, 6})) == CommandError::Ok);
    CHECK(!g->battlePending());  // resolved with Civ math at once
    CHECK(g->state().unit(leader) == nullptr);
    // Sunk, not taken: no captor, and the succession starts.
    CHECK_EQ(g->state().players[0].captor, kNoPlayer);
    CHECK(g->state().players[0].successionPending);
}

// Open gaps review 8: barbarians wound the leader in a live battle too, and a badly hurt leader goes home.
TEST(a_leader_hurt_in_a_live_battle_with_barbarians_goes_home) {
    GameState s = flatState(16, 12, 2);
    s.setup.liveBattles = true;
    addCity(s, 0, {2, 2}, true);
    addCity(s, 1, {13, 9}, true);
    const UnitId leader = addLeader(s, 0, {6, 6});
    s.units.back().hp = 40;
    const UnitId escort = addUnit(s, "UNIT_SWORDSMAN", 0, {6, 6});
    Player b;
    b.id = 2;
    b.barbarian = true;
    s.players.push_back(b);
    addUnit(s, "UNIT_SWORDSMAN", 2, {7, 6});
    for (Player& p : s.players) p.human = p.id == 0;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->atWar(0, 2));
    REQUIRE(g->submit(Command::attack(0, escort, {7, 6})) == CommandError::Ok);
    REQUIRE(g->battlePending());
    REQUIRE(g->submit(Command::battleResult(0, 50, 50, 30)) == CommandError::Ok);
    const Unit* l = g->state().unit(leader);
    REQUIRE(l);
    CHECK_EQ(l->hp, 10);
    CHECK(l->pos == hx(2, 2));
}

TEST(an_assault_from_the_sea_on_the_leaders_city_auto_resolves) {
    Siege f = siege(true, true);
    Game& g = *f.game;
    REQUIRE(g.state().currentPlayer == 1);
    const int hp = g.state().city(f.city)->hp;
    REQUIRE(g.submit(Command::attack(1, f.attacker, {6, 6})) == CommandError::Ok);
    CHECK(!g.battlePending());
    CHECK(g.state().city(f.city)->hp < hp);
}
