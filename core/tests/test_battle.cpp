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
