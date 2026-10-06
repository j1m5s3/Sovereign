// Natural wonders (01-map-and-terrain.md: Natural wonders).
#include <algorithm>

#include "helpers.h"
#include "sovereign/mapgen.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
}

TEST(natural_wonders_from_the_data) {
    const Rules& r = rules();
    const FeatureType& reef = r.features[at(r.feature("FEATURE_GREAT_BARRIER_REEF"))];
    CHECK(reef.naturalWonder);
    CHECK_EQ(reef.tiles, 2);
    CHECK(reef.yields[static_cast<size_t>(YieldType::Food)] == Fixed::fromInt(3));
    const FeatureType& everest = r.features[at(r.feature("FEATURE_MOUNT_EVEREST"))];
    CHECK(everest.impassable);
    CHECK(everest.adjacentYields[static_cast<size_t>(YieldType::Faith)] == Fixed::fromInt(1));
    CHECK(r.features[at(r.feature("FEATURE_TORRES_DEL_PAINE"))].doublesAdjacentTerrain);
}

TEST(a_natural_wonder_feeds_its_neighbours_and_blocks_building) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {6, 6}, true, 3);
    auto plain = Game::fromScenario(rules(), s);
    const Yields before = plain->plotYields({7, 6}, plain->state().cities[0]);
    const TypeIndex uluru = rules().feature("FEATURE_ULURU");
    s.plot({8, 6}).terrain = rules().terrain("TERRAIN_DESERT");
    s.plot({8, 6}).feature = uluru;
    auto g = Game::fromScenario(rules(), std::move(s));
    const Yields after = g->plotYields({7, 6}, g->state().cities[0]);
    CHECK(after[static_cast<size_t>(YieldType::Faith)] == before[static_cast<size_t>(YieldType::Faith)] + Fixed::fromInt(2));
    CHECK(after[static_cast<size_t>(YieldType::Culture)] == before[static_cast<size_t>(YieldType::Culture)] + Fixed::fromInt(2));
    CHECK(!isLandPassable(g->state(), rules(), {8, 6}));  // impassable
    CHECK(!g->canFoundCityAt(0, {8, 6}));
}

TEST(the_map_script_places_natural_wonders) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(7);
    setup.mapSize = "MAPSIZE_SMALL";
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    std::vector<TypeIndex> seen;
    for (const Plot& p : g->state().plots) {
        if (p.feature != kNone && rules().features[at(p.feature)].naturalWonder &&
            std::find(seen.begin(), seen.end(), p.feature) == seen.end())
            seen.push_back(p.feature);
    }
    CHECK(seen.size() >= 1u);
    CHECK(seen.size() <= 4u);  // Small: 4
}

TEST(natural_wonder_effects) {
    // Giant's Causeway: +5 to a land unit beside it.
    GameState s = flatState(16, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const UnitId a = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
    const UnitId d = sovtest::addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    auto plain = Game::fromScenario(rules(), s);
    const int base = plain->combatStrength(*plain->state().unit(a), *plain->state().unit(d), true, false);
    s.plot({4, 5}).feature = rules().feature("FEATURE_GIANT_S_CAUSEWAY");
    auto g = Game::fromScenario(rules(), s);
    CHECK_EQ(g->combatStrength(*g->state().unit(a), *g->state().unit(d), true, false), base + 5);

    // Pamukkale: its city gains an amenity per natural wonder in its land (itself and Uluru here).
    GameState t = flatState(16, 12, 1);
    Game::fitPlayerToRules(t.players[0], rules());
    addCity(t, 0, {6, 6}, true, 3);
    auto none = Game::fromScenario(rules(), t);
    const int amen = none->cityReport(none->state().cities[0].id).amenities;
    t.plot({7, 6}).feature = rules().feature("FEATURE_PAMUKKALE");
    t.plot({5, 6}).terrain = rules().terrain("TERRAIN_DESERT");
    t.plot({5, 6}).feature = rules().feature("FEATURE_ULURU");
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->cityReport(h->state().cities[0].id).amenities, amen + 2);
}

TEST(wonders_leave_permanent_marks_and_reward_their_discovery) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 2;
    addCity(s, 0, {2, 2}, true, 3);
    addCity(s, 1, {18, 10}, true, 3);
    // The Fountain of Youth at (8,6); Everest at (8,9) beside hills at (7,9).
    s.plot({8, 6}).feature = rules().feature("FEATURE_FOUNTAIN_OF_YOUTH");
    s.plot({8, 9}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    s.plot({8, 9}).feature = rules().feature("FEATURE_MOUNT_EVEREST");
    s.plot({6, 9}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    const UnitId scout = sovtest::addUnit(s, "UNIT_SCOUT", 0, {5, 6});
    const UnitId warrior = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {5, 9});
    // The map known, but for the Fountain of Youth.
    for (Player& p : s.players) {
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        p.visibility[static_cast<size_t>(s.grid.index({8, 6}))] = static_cast<uint8_t>(Visibility::Unrevealed);
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    const int score = g->state().players[0].eraScore;
    g->stateMutForTests().units[0].xp = 0;
    REQUIRE(g->submit(Command::move(0, scout, {6, 6})) == CommandError::Ok);  // now it sees the Fountain of Youth: the world's first discovery
    CHECK(g->state().players[0].eraScore > score);
    CHECK(g->state().players[0].techs.boosted[at(rules().tech("TECH_ASTROLOGY"))]);
    CHECK_EQ(g->state().unit(scout)->xp, rules().globalInt("EXPERIENCE_REVEAL_NATURAL_WONDER"));
    // Entering the Fountain: +10 HP healing for good. Beside Everest: hills cost as flat ground.
    const auto before = g->moveCost(*g->state().unit(warrior), {5, 9}, {6, 9});
    REQUIRE(g->submit(Command::move(0, scout, {8, 6}, true)) == CommandError::Ok);
    REQUIRE(g->submit(Command::move(0, warrior, {7, 9}, true)) == CommandError::Ok);
    for (int i = 0; i < 3 && !(g->state().unit(warrior)->pos == Hex{7, 9} && g->state().unit(scout)->pos == Hex{8, 6}); ++i) sovtest::endTurns(*g, 2);
    CHECK(g->state().unit(scout)->wonderAbilities & 2);
    REQUIRE((g->state().unit(warrior)->pos == Hex{7, 9}));
    CHECK(g->state().unit(warrior)->wonderAbilities & 1);
    const auto after = g->moveCost(*g->state().unit(warrior), {7, 9}, {6, 9});
    REQUIRE(before && after);
    CHECK(*after < *before);
}
