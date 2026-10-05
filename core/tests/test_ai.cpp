// The computer opponent (MVP-6; 10-ai-ui-implementation.md, AI architecture).
#include <algorithm>
#include <string>

#include "../tools/random_bot.h"
#include "helpers.h"
#include "sovereign/ai.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int citiesOf(const Game& g, PlayerId p) {
    int n = 0;
    for (const City& c : g.state().cities) n += c.owner == p;
    return n;
}

int populationOf(const Game& g, PlayerId p) {
    int n = 0;
    for (const City& c : g.state().cities) n += c.owner == p ? c.population : 0;
    return n;
}

bool capitalTaken(const Game& g) {
    for (const City& c : g.state().cities) {
        if (c.originalCapital && c.owner != c.originalOwner) return true;
    }
    return false;
}

void learn(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[at(p)];
    pl.techs.resize(rules().techs.size());
    pl.techs.done[at(rules().tech(tech))] = 1;
}
}  // namespace

TEST(ai_founds_capital_and_fills_every_order) {
    GameState s = flatState(20, 14, 1);
    addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
    addUnit(s, "UNIT_BUILDER", 0, {6, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    const int turn = g->state().turn;
    ai::playTurn(*g);
    CHECK_EQ(g->state().turn, turn + 1);  // the turn ended: nothing was left without orders
    REQUIRE(citiesOf(*g, 0) == 1);
    const City& capital = g->state().cities.front();
    CHECK_EQ(capital.pos, (Hex{6, 6}));
    CHECK(!capital.queue.empty());
    const Player& p = g->state().players[0];
    CHECK(p.techs.current != kNone);
    CHECK(p.civics.current != kNone);
    // A few more turns: the Builder improves a plot and the city keeps producing.
    for (int i = 0; i < 6; ++i) ai::playTurn(*g);
    int improved = 0;
    for (const Plot& pl : g->state().plots) improved += pl.improvement != kNone;
    CHECK(improved >= 1);
    CHECK(!g->state().cities.front().queue.empty());
}

TEST(ai_settle_score_prefers_good_sites) {
    GameState s = flatState(24, 14, 1);
    addCity(s, 0, {4, 6}, true);
    // A river at (12,6); (18,6) is plain grassland as far from the capital. Desert around (12,9).
    s.plot({12, 6}).riverEdges = kRiverE;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(ai::settleScore(*g, 0, {5, 6}), -1);  // too close to the capital
    CHECK(ai::settleScore(*g, 0, {8, 6}) > ai::settleScore(*g, 0, {12, 7}));  // nearer home
    CHECK(ai::settleScore(*g, 0, {12, 6}) > ai::settleScore(*g, 0, {12, 8}));  // fresh water
    GameState d = flatState(24, 14, 1);
    for (const Hex& h : d.grid.within({12, 6}, 3)) d.plot(h).terrain = rules().terrain("TERRAIN_DESERT");
    auto dry = Game::fromScenario(rules(), std::move(d));
    CHECK(ai::settleScore(*dry, 0, {12, 6}) < ai::settleScore(*dry, 0, {6, 6}));
}

TEST(ai_wins_a_fight_it_should_win) {
    GameState s = flatState(20, 14, 2);
    addCity(s, 0, {4, 6}, true);
    addCity(s, 1, {15, 6}, true);
    const UnitId mine = addUnit(s, "UNIT_WARRIOR", 0, {9, 6});
    const UnitId theirs = addUnit(s, "UNIT_WARRIOR", 1, {10, 6});
    s.unit(theirs)->hp = 20;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->state().currentPlayer == 0);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    ai::playTurn(*g);
    CHECK(g->state().unit(theirs) == nullptr);  // a sure kill is taken
    CHECK(g->state().unit(mine) != nullptr);
}

TEST(ai_declares_war_on_a_weak_neighbour) {
    GameState s = flatState(24, 14, 2);
    addCity(s, 0, {4, 6}, true);
    addCity(s, 0, {4, 10}, false);
    addCity(s, 1, {12, 6}, true);
    learn(s, 0, "TECH_BRONZE_WORKING");
    for (int i = 0; i < 6; ++i) addUnit(s, "UNIT_WARRIOR", 0, {static_cast<int32_t>(3 + i), 3});
    addUnit(s, "UNIT_WARRIOR", 1, {12, 6});
    s.turn = 60;
    GameState far = s;
    addUnit(s, "UNIT_SCOUT", 0, {10, 6});  // has seen the neighbour's city
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->visibility(0, {12, 6}) != Visibility::Unrevealed);
    ai::playTurn(*g);
    CHECK(g->atWar(0, 1));
    // A neighbour it has never seen is left alone.
    auto blind = Game::fromScenario(rules(), std::move(far));
    REQUIRE(blind->visibility(0, {12, 6}) == Visibility::Unrevealed);
    ai::playTurn(*blind);
    CHECK(!blind->atWar(0, 1));
}

TEST(ai_beats_the_random_bot) {
    GameSetup setup;
    setup.seed = 3;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    Rng botRng(99);
    while (g->state().turn < 120 && !g->gameOver()) {
        if (g->state().currentPlayer < 2) ai::playTurn(*g);
        else sovbot::playTurn(*g, botRng);
    }
    const int aiCities = citiesOf(*g, 0) + citiesOf(*g, 1), botCities = citiesOf(*g, 2) + citiesOf(*g, 3);
    const int aiPop = populationOf(*g, 0) + populationOf(*g, 1), botPop = populationOf(*g, 2) + populationOf(*g, 3);
    CHECK(aiCities > botCities);
    CHECK(aiPop > botPop);
}

TEST(ai_soak_takes_a_capital_and_replays) {
    GameSetup setup;
    setup.seed = 2;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    while (g->state().turn < 250 && !capitalTaken(*g) && !g->gameOver()) ai::playTurn(*g);
    CHECK(capitalTaken(*g));
    auto again = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(again);
    CHECK_EQ(again->stateHash(), g->stateHash());
}

namespace {
bool holds(const Game& g, PlayerId p, ai::Strategy s) {
    const std::vector<ai::Strategy> on = ai::strategies(g, p);
    return std::find(on.begin(), on.end(), s) != on.end();
}
}  // namespace

TEST(ai_grand_strategy_rapid_expansion_and_dark_age) {
    GameState s = flatState(30, 20, 1);
    addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);  // founds the capital
    REQUIRE(citiesOf(*g, 0) == 1);
    GameState s1 = g->state();
    for (uint8_t& v : s1.players[0].visibility) v = std::max(v, static_cast<uint8_t>(Visibility::Revealed));  // open land all round, explored
    auto g1 = Game::fromScenario(rules(), s1);
    CHECK(holds(*g1, 0, ai::Strategy::RapidExpansion));
    CHECK(!holds(*g1, 0, ai::Strategy::ScienceVictory));  // no victory strategy in the Ancient era
    GameState s2 = s1;
    s2.players[0].age = Age::Dark;
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK(holds(*g2, 0, ai::Strategy::DarkAge));
    CHECK(!holds(*g2, 0, ai::Strategy::RapidExpansion));  // Rapid Expansion is forbidden in a Dark Age
    CHECK_EQ(std::string(ai::strategyName(ai::Strategy::DarkAge)), std::string("Dark Age"));
}

TEST(ai_grand_strategy_domination_after_taking_a_capital) {
    GameState s = flatState(30, 14, 3);
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 0, {10, 6}, false, 4);
    s.cities.back().originalOwner = 1;
    s.cities.back().originalCapital = true;  // player 1's old capital
    addCity(s, 1, {18, 6}, false, 3);
    addCity(s, 2, {25, 6}, true, 3);
    for (int i = 0; i < 4; ++i) addUnit(s, "UNIT_SWORDSMAN", 0, {4, 8 + i % 2});
    s.gameEra = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(holds(*g, 0, ai::Strategy::DominationVictory));  // took a capital, two rivals, the strongest army
    CHECK(!holds(*g, 1, ai::Strategy::DominationVictory));
}

TEST(ai_grand_strategy_agendas) {
    TypeIndex qin = kNone;
    for (size_t i = 0; i < rules().civs.size(); ++i) {
        if (rules().civs[i].agenda == Agenda::FirstEmperor) qin = static_cast<TypeIndex>(i);
    }
    REQUIRE(qin != kNone);
    GameState s = flatState(20, 14, 1);
    s.players[0].civ = qin;
    addCity(s, 0, {6, 6}, true, 3);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(holds(*g, 0, ai::Strategy::WonderObsessed));
}
