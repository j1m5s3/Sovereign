// City-state quests (08-diplomacy-city-states-governors.md: Quests).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

// Majors 0 and 1, city-state 2 at (16,6); all have met.
GameState world() {
    GameState s = flatState(24, 14, 3);
    s.players[2].civ = kNone;
    for (size_t i = 0; i < rules().cityStates.size(); ++i) {
        if (rules().cityStates[i].kind == CityStateKind::Scientific) s.players[2].cityState = static_cast<TypeIndex>(i);
    }
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(3, 0);
        p.met.assign(3, uint8_t{1});
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        p.techs.done[at(rules().tech("TECH_BRONZE_WORKING"))] = 1;
    }
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 1, {10, 2}, true, 3);
    addCity(s, 2, {16, 6}, true, 2);
    s.players[2].firstMetBy = 1;
    s.majorsAtStart = 2;
    return s;
}
}  // namespace

TEST(each_major_gets_a_quest_and_fulfilling_it_brings_an_envoy) {
    auto g = Game::fromScenario(rules(), world());
    g->assignQuests();
    const Quest* q0 = g->questFor(2, 0);
    const Quest* q1 = g->questFor(2, 1);
    REQUIRE(q0);
    REQUIRE(q1);
    CHECK(g->questFor(2, 2) == nullptr);  // not for the city-state itself
    CHECK(!g->questText(*q0).empty());
    const Quest copy = *q0;
    g->assignQuests();  // one open quest at a time
    CHECK_EQ(g->state().quests.size(), 2u);
    const int before = g->envoysAt(0, 2);
    g->questDone(0, copy.kind, copy.arg + 1000);  // not the one asked for
    CHECK_EQ(g->envoysAt(0, 2), before);
    g->questDone(0, copy.kind, copy.arg);
    CHECK_EQ(g->envoysAt(0, 2), before + 1);
    CHECK(g->questFor(2, 0) == nullptr);
    CHECK(g->questFor(2, 1) != nullptr);
}

TEST(a_trade_route_fulfils_its_quest) {
    GameState s = world();
    s.quests.push_back({2, 0, QuestKind::TradeRoute, 2});
    TradeRoute r;
    r.owner = 0;
    r.origin = s.cities[0].id;
    r.destination = s.cities[2].id;
    s.tradeRoutes.push_back(r);
    auto g = Game::fromScenario(rules(), std::move(s));
    g->checkQuests();
    CHECK_EQ(g->envoysAt(0, 2), 1);
    CHECK(g->state().quests.empty());
}

TEST(quests_survive_a_save) {
    GameState s = world();
    s.quests.push_back({2, 1, QuestKind::TrainUnit, rules().unit("UNIT_WARRIOR")});
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->state().quests.size() == 1u);
    CHECK(loaded->state().quests[0].kind == QuestKind::TrainUnit);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
