// Archaeology (07-great-people-great-works-tourism.md: Archaeology).
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

TEST(battles_become_antiquity_sites_and_archaeologists_dig_them) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 2;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {14, 6}, true, 6);
    s.battleSites = {s.grid.index({6, 6})};
    City& home = s.cities[0];
    home.buildings.push_back(rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM"));
    std::sort(home.buildings.begin(), home.buildings.end());
    auto early = Game::fromScenario(rules(), s);
    early->placeAntiquity();
    CHECK(!early->state().antiquityPlaced);  // nobody knows Natural History yet
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    const UnitId dig = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->state().unit(dig)->charges, 3);
    g->placeAntiquity();
    REQUIRE(g->state().antiquityPlaced);
    CHECK_EQ(g->state().plot({6, 6}).antiquity, 1);  // the battle plot first
    int land = 0;
    for (const Plot& p : g->state().plots) land += p.antiquity == 1 ? 1 : 0;
    CHECK_EQ(land, 2 * rules().globalInt("ARCHAEOLOGY_SITES_PER_CIV_LAND"));
    const size_t works = g->state().cities[0].greatWorks.size();
    REQUIRE(g->submit(Command::excavate(0, dig)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).antiquity, 0);
    CHECK_EQ(g->state().cities[0].greatWorks.size(), works + 1);
    CHECK_EQ(g->state().unit(dig)->charges, 2);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
