// World wonders (03-districts-buildings-wonders.md, Wonders).
#include "helpers.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex wonder(const char* id) { return rules().building(id); }

void giveTech(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[at(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.techs.done[at(rules().tech(tech))] = 1;
}

// Two players with a city each on desert land; both know Masonry (Pyramids) and Irrigation.
GameState wonderState() {
    GameState s = flatState(24, 14, 2);
    for (Plot& p : s.plots) p.terrain = rules().terrain("TERRAIN_DESERT");
    for (PlayerId p = 0; p < 2; ++p) {
        giveTech(s, p, "TECH_MASONRY");
        giveTech(s, p, "TECH_IRRIGATION");
    }
    const CityId a = addCity(s, 0, {5, 6}, true, 3);
    const CityId b = addCity(s, 1, {15, 6}, true, 3);
    for (const Hex& h : s.grid.within({5, 6}, 3)) s.plot(h).city = a, s.plot(h).owner = 0;
    for (const Hex& h : s.grid.within({15, 6}, 3)) s.plot(h).city = b, s.plot(h).owner = 1;
    return s;
}
}  // namespace

TEST(wonder_rules_data) {
    const Rules& r = rules();
    int wonders = 0;
    for (const BuildingType& b : r.buildings) wonders += b.wonder;
    CHECK_EQ(wonders, 47);
    const BuildingType& stonehenge = r.buildings[at(wonder("BUILDING_STONEHENGE"))];
    CHECK_EQ(stonehenge.placement.nextToResource, r.resource("RESOURCE_STONE"));
    CHECK_EQ(stonehenge.placement.terrains.size(), 5u);
    const BuildingType& colossus = r.buildings[at(wonder("BUILDING_COLOSSUS"))];
    CHECK_EQ(colossus.tradeCapacity, 1);
    REQUIRE(colossus.wonderEffects.size() == 1u);
    CHECK_EQ(colossus.wonderEffects[0].ref, r.unit("UNIT_TRADER"));
    CHECK(r.buildings[at(wonder("BUILDING_HANGING_GARDENS"))].placement.river);
}

TEST(wonders_need_their_ground) {
    GameState s = wonderState();
    setRiver(s, {6, 6}, Dir::E);
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& c = g->state().cities[0];
    const TypeIndex gardens = wonder("BUILDING_HANGING_GARDENS");
    CHECK(g->canPlaceWonder(c, gardens, {6, 6}));   // on the river
    CHECK(!g->canPlaceWonder(c, gardens, {4, 6}));  // dry land
    CHECK(!g->canPlaceWonder(c, gardens, {5, 6}));  // the city itself
    CHECK(g->canPlaceWonder(c, wonder("BUILDING_PYRAMIDS"), {4, 6}));  // desert
}

TEST(a_wonder_is_built_once_and_rivals_keep_half) {
    GameState s = wonderState();
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex pyramids = wonder("BUILDING_PYRAMIDS");
    const CityId mine = g->state().cities[0].id, theirs = g->state().cities[1].id;
    const ProductionItem item{ProductionKind::Building, pyramids};
    REQUIRE(g->submit(Command::setProduction(0, mine, item, {4, 6})) == CommandError::Ok);
    CHECK_EQ(g->state().wonderAt({4, 6}), pyramids);
    sovtest::endTurns(*g, 1);
    REQUIRE(g->submit(Command::setProduction(1, theirs, item, {16, 6})) == CommandError::Ok);
    // Nearly done in our city, half-way in theirs.
    GameState mid = g->state();
    const int cost = g->productionCost(0, item);
    mid.city(mine)->progress.push_back({item, Fixed::fromInt(cost - 1)});
    mid.city(theirs)->progress.push_back({item, Fixed::fromInt(100)});
    auto g2 = Game::fromScenario(rules(), std::move(mid));
    const size_t units = g2->state().units.size();
    sovtest::endTurns(*g2, 2);  // round to our turn: the Pyramids complete
    CHECK(g2->wonderBuilt(pyramids));
    CHECK(g2->state().city(mine)->has(pyramids));
    CHECK_EQ(g2->state().units.size(), units + 1);  // the free Builder
    const City& rival = *g2->state().city(theirs);
    CHECK(std::find(rival.queue.begin(), rival.queue.end(), item) == rival.queue.end());
    CHECK(rival.wonders.empty());
    CHECK(rival.overflow >= Fixed::fromInt(50));
    CHECK(!g2->canProduce(rival, item));
}

TEST(wonder_effects_reach_every_city) {
    GameState s = wonderState();
    s.cities[0].buildings.push_back(wonder("BUILDING_HANGING_GARDENS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    // +15% growth in all of the owner's cities.
    CHECK_EQ(sumCityModifiers(g->state(), rules(), g->state().cities[0], ModEffect::CityGrowthPercent), Fixed::fromInt(15));
    CHECK_EQ(sumCityModifiers(g->state(), rules(), g->state().cities[1], ModEffect::CityGrowthPercent), Fixed());
}

TEST(wonder_sites_survive_a_save) {
    GameState s = wonderState();
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::setProduction(0, g->state().cities[0].id, {ProductionKind::Building, wonder("BUILDING_PYRAMIDS")}, {4, 6})) ==
            CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().cities[0].wonders.size(), 1u);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
