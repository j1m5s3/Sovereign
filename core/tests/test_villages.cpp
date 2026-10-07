// Tribal villages (01-map-and-terrain.md: Tribal Villages).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

TEST(village_rewards_from_the_data) {
    const Rules& r = rules();
    REQUIRE(!r.goodies.empty());
    bool gold = false, builder = false, eureka = false;
    for (const GoodyType& g : r.goodies) {
        gold = gold || (g.kind == GoodyKind::Gold && g.amount == 40);
        builder = builder || (g.kind == GoodyKind::Unit && g.unit == r.unit("UNIT_BUILDER"));
        eureka = eureka || g.kind == GoodyKind::Eureka;
    }
    CHECK(gold);
    CHECK(builder);
    CHECK(eureka);
}

TEST(entering_a_village_consumes_it_for_a_reward) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {4, 6}, true, 3);
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
    s.plot({7, 6}).village = true;
    auto g = Game::fromScenario(rules(), s);
    const int xp = g->state().unit(scout)->xp;
    REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
    CHECK(!g->state().plot({7, 6}).village);
    CHECK_EQ(g->state().unit(scout)->xp >= xp + rules().globalInt("EXPERIENCE_ACTIVATE_GOODY_HUT"), true);
    REQUIRE(!g->state().events.empty());
    CHECK(g->state().events.back().kind == EventKind::GoodyHut);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(a_village_contacted_in_the_ancient_era_is_a_moment) {
    // 09: Tribal Village Contacted counts only while the world is in the Ancient Era; the camp moments until the Medieval.
    const auto contacted = [](int era) {
        GameState s = flatState(16, 12, 1);
        Game::fitPlayerToRules(s.players[0], rules());
        addCity(s, 0, {4, 6}, true, 3);
        const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
        s.plot({7, 6}).village = true;
        s.gameEra = era;
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
        return sovtest::hasMoment(*g, 0, "MOMENT_TRIBAL_VILLAGE_CONTACTED");
    };
    CHECK(contacted(0));
    CHECK(!contacted(1));
    const MomentType& camp = rules().moments[static_cast<size_t>(rules().moment("MOMENT_BARBARIAN_CAMP_DESTROYED"))];
    CHECK_EQ(camp.eraMin, static_cast<int>(rules().era("ERA_ANCIENT")));
    CHECK_EQ(camp.eraMax, static_cast<int>(rules().era("ERA_MEDIEVAL")));
}

TEST(the_map_script_scatters_villages) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(5);
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    int villages = 0;
    for (const Plot& p : g->state().plots) villages += p.village ? 1 : 0;
    CHECK(villages > 0);
    for (int i = 0; i < g->state().grid.size(); ++i) {
        if (!g->state().plots[static_cast<size_t>(i)].village) continue;
        for (const Player& p : g->state().players) CHECK(g->state().grid.distance(p.startPos, g->state().grid.at(i)) >= 4);
    }
    setup.tribalVillages = false;
    auto none = Game::create(rules(), setup, &err);
    REQUIRE(none);
    int left = 0;
    for (const Plot& p : none->state().plots) left += p.village ? 1 : 0;
    CHECK_EQ(left, 0);
}
