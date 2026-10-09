#include <algorithm>
#include <array>
#include <utility>

#include "helpers.h"
#include "sovereign/mapgen.h"

using namespace sov;
using sovtest::rules;

TEST(map_generation_is_deterministic) {
    std::string err;
    auto a = Game::create(rules(), sovtest::duelSetup(42), &err);
    auto b = Game::create(rules(), sovtest::duelSetup(42), &err);
    auto c = Game::create(rules(), sovtest::duelSetup(43), &err);
    REQUIRE(a && b && c);
    CHECK_EQ(a->stateHash(), b->stateHash());
    CHECK(a->stateHash() != c->stateHash());
}

TEST(map_has_sensible_shape) {
    const Rules& r = rules();
    for (uint64_t seed : {1ull, 2ull, 3ull, 99ull}) {
        std::string err;
        auto g = Game::create(r, sovtest::duelSetup(seed), &err);
        REQUIRE(g);
        const GameState& s = g->state();
        int land = 0, mountains = 0, rivers = 0, features = 0, resources = 0;
        for (int i = 0; i < s.grid.size(); ++i) {
            const Plot& p = s.plots[static_cast<size_t>(i)];
            const TerrainType& t = r.terrains[static_cast<size_t>(p.terrain)];
            if (!t.water) ++land;
            if (t.relief == Relief::Mountain) ++mountains;
            if (p.riverEdges) ++rivers;
            if (p.feature != kNone) {
                ++features;
                const auto& valid = r.features[static_cast<size_t>(p.feature)].validTerrains;
                CHECK(std::find(valid.begin(), valid.end(), p.terrain) != valid.end());
            }
            if (p.resource != kNone) ++resources;
            // Polar rows are never land.
            if (i < s.grid.width() || i >= s.grid.size() - s.grid.width()) CHECK(t.water);
        }
        int pct = land * 100 / s.grid.size();
        CHECK(pct >= 32 && pct <= 40);
        CHECK(mountains > 0);
        CHECK(rivers > 0);
        CHECK(features > 0);
        CHECK(resources > 0);
        // Water next to land is coast, never ocean.
        for (int i = 0; i < s.grid.size(); ++i) {
            Hex h = s.grid.at(i);
            if (s.plot(h).terrain != r.terrain("TERRAIN_OCEAN")) continue;
            for (const Hex& n : s.grid.within(h, 1)) CHECK(r.terrains[static_cast<size_t>(s.plot(n).terrain)].water);
        }
    }
}

TEST(map_start_positions_are_spaced_and_valid) {
    const Rules& r = rules();
    GameSetup setup = sovtest::duelSetup(5);
    setup.mapSize = "MAPSIZE_TINY";
    setup.players = {{"CIVILIZATION_ROME", true}, {"CIVILIZATION_EGYPT", false}, {"CIVILIZATION_CHINA", false},
                     {"CIVILIZATION_INCA", false}};
    std::string err;
    auto g = Game::create(r, setup, &err);
    REQUIRE(g);
    auto ps = g->state().players;
    CHECK(ps.back().barbarian);  // the barbarians have no start position
    ps.pop_back();
    for (size_t i = 0; i < ps.size(); ++i) {
        CHECK(isLandPassable(g->state(), r, ps[i].startPos));
        for (size_t j = i + 1; j < ps.size(); ++j)
            CHECK(g->state().grid.distance(ps[i].startPos, ps[j].startPos) > r.globalInt("CITY_MIN_RANGE"));
    }
}

// The best-scoring start plots go to the players in a shuffled order. Handed out best first, the first seat (the
// human's) always had the best start: by turn 150 of all-AI games it made 60% more science than the last.
TEST(map_the_best_start_goes_to_any_seat) {
    const Rules& r = rules();
    std::vector<int> best(6, 0);  // games in which each seat holds the best-scoring start
    for (uint64_t seed = 1; seed <= 24; ++seed) {
        GameSetup setup;
        setup.seed = seed;
        setup.mapSize = "MAPSIZE_SMALL";
        setup.cityStates = 0;
        for (size_t i = 0; i < 6; ++i) setup.players.push_back({r.civs[i].id, false});
        std::string err;
        auto g = Game::create(r, setup, &err);
        REQUIRE(g);
        size_t seat = 0;
        for (size_t i = 1; i < 6; ++i) {
            if (startScore(g->state(), r, g->state().players[i].startPos) > startScore(g->state(), r, g->state().players[seat].startPos)) seat = i;
        }
        ++best[seat];
    }
    CHECK(best[0] < 12);
    CHECK(best[5] > 0);
}

// A major civ's start short of Food or Production on the plots beside it gets one Bonus resource of each kind there
// (01, step 7): under 7 Food or no plot with 3, under 5 Production or no plot with 2.
TEST(map_a_poor_start_gets_food_and_production_beside_it) {
    const Rules& r = rules();
    const Hex start{5, 5};
    // A start on grassland with these plots beside it (Cattle on the first if asked, woods on all if asked), given its
    // bonuses: the resources beside it that give Food, and those that give Production. Nothing else on the map has one,
    // not even beside a city-state's start on bare grassland.
    const auto bonuses = [&](std::array<const char*, kNumDirs> ring, bool cattle, bool woods = false) {
        GameState s = sovtest::flatState(12, 12, 2);
        s.players[0].startPos = start;
        s.players[1].cityState = 0;
        s.players[1].startPos = {9, 9};
        for (int d = 0; d < kNumDirs; ++d) {
            Plot& p = s.plot(*s.grid.neighbor(start, static_cast<Dir>(d)));
            p.terrain = r.terrain(ring[static_cast<size_t>(d)]);
            if (woods) p.feature = r.feature("FEATURE_FOREST");
        }
        if (cattle) s.plot(*s.grid.neighbor(start, static_cast<Dir>(0))).resource = r.resource("RESOURCE_CATTLE");
        addStartBonuses(s, r);
        std::pair<int, int> n{0, 0};
        for (int i = 0; i < s.grid.size(); ++i) {
            const Plot& p = s.plots[static_cast<size_t>(i)];
            if (p.resource == kNone) continue;
            CHECK_EQ(s.grid.distance(s.grid.at(i), start), 1);
            const Yields& y = r.resources[static_cast<size_t>(p.resource)].yields;
            n.first += y[static_cast<size_t>(YieldType::Food)] > Fixed() ? 1 : 0;
            n.second += y[static_cast<size_t>(YieldType::Production)] > Fixed() ? 1 : 0;
        }
        return n;
    };
    const char* grass = "TERRAIN_GRASS";
    const char* plains = "TERRAIN_PLAINS";
    const char* plainsHills = "TERRAIN_PLAINS_HILLS";
    const char* desert = "TERRAIN_DESERT";
    const char* desertHills = "TERRAIN_DESERT_HILLS";
    const char* grassHills = "TERRAIN_GRASS_HILLS";
    using Got = std::pair<int, int>;
    // Grassland: 12 Food, but no plot with 3, and no Production.
    CHECK((bonuses({grass, grass, grass, grass, grass, grass}, false) == Got{1, 1}));
    // Plains: 6 Food and 6 Production, no plot with 3 or 2; Wheat fits, no Production resource does.
    CHECK((bonuses({plains, plains, plains, plains, plains, plains}, false) == Got{1, 0}));
    // Just enough: Cattle on grassland (3 Food), grassland, two plains hills (1 Food, 2 Production), desert hills
    // (1 Production) and desert: 7 Food and 5 Production, with grassland and desert left to take more.
    CHECK((bonuses({grass, grass, plainsHills, plainsHills, desertHills, desert}, true) == Got{1, 0}));
    // Cattle and desert hills: a plot with 3 Food but 3 in all (Sheep fits the hills); no plot with 2 Production.
    CHECK((bonuses({grass, desertHills, desertHills, desertHills, desertHills, desertHills}, true) == Got{2, 0}));
    // Cattle, three plains hills and desert: a plot with 3 Food but 6 in all (Sheep fits the hills).
    CHECK((bonuses({grass, plainsHills, plainsHills, plainsHills, desert, desertHills}, true) == Got{2, 0}));
    // Two plains hills and grassland: a plot with 2 Production but 4 in all (Stone fits the grassland).
    CHECK((bonuses({plainsHills, plainsHills, grass, grass, grass, grass}, false) == Got{1, 1}));
    // Grassland hills and grassland: 5 Production, but no plot with 2 (Stone fits either).
    CHECK((bonuses({grassHills, grassHills, grassHills, grassHills, grassHills, grass}, false) == Got{1, 1}));
    // Wooded grassland: the woods decide what fits, so Deer and no Food resource (Cattle and Rice need open grassland).
    CHECK((bonuses({grass, grass, grass, grass, grass, grass}, false, true) == Got{0, 1}));
}

TEST(map_river_edges_are_shared) {
    GameState s = sovtest::flatState(8, 8, 1);
    setRiver(s, {3, 3}, Dir::W);
    CHECK(hasRiver(s, {3, 3}, Dir::W));
    CHECK(hasRiver(s, {2, 3}, Dir::E));
    CHECK(!hasRiver(s, {3, 3}, Dir::E));
    setRiver(s, {3, 3}, Dir::NE);
    CHECK(hasRiver(s, *s.grid.neighbor({3, 3}, Dir::NE), Dir::SW));
    CHECK(isRiverAdjacent(s, {3, 3}));
    CHECK(!isRiverAdjacent(s, {6, 6}));
}

// A river on any one of a plot's six edges makes it and the plot across that edge river-adjacent, and no other.
TEST(map_a_river_on_any_edge_is_adjacent) {
    for (int d = 0; d < kNumDirs; ++d) {
        GameState s = sovtest::flatState(8, 8, 1);
        setRiver(s, {3, 3}, static_cast<Dir>(d));
        int adjacent = 0;
        for (int i = 0; i < s.grid.size(); ++i) adjacent += isRiverAdjacent(s, s.grid.at(i)) ? 1 : 0;
        CHECK_EQ(adjacent, 2);
        CHECK(isRiverAdjacent(s, {3, 3}));
        CHECK(isRiverAdjacent(s, *s.grid.neighbor({3, 3}, static_cast<Dir>(d))));
    }
}

namespace {
// lakeMap marks the plots isLake finds, and the checks given it answer as they do without it.
bool lakeMapAgrees(const GameState& s, const Rules& r) {
    const std::vector<uint8_t> lakes = lakeMap(s, r);
    if (lakes.size() != s.plots.size()) return false;
    for (int i = 0; i < s.grid.size(); ++i) {
        const Hex h = s.grid.at(i);
        if ((lakes[static_cast<size_t>(i)] != 0) != isLake(s, r, h) || isLake(s, r, h, &lakes) != isLake(s, r, h) ||
            isLakeAdjacent(s, r, h, &lakes) != isLakeAdjacent(s, r, h) || hasFreshWater(s, r, h, &lakes) != hasFreshWater(s, r, h))
            return false;
    }
    return true;
}
}  // namespace

TEST(lakes_are_small_bodies_of_water) {
    GameState s = sovtest::flatState(16, 16, 1);
    const Rules& r = rules();
    const TypeIndex coast = r.terrain("TERRAIN_COAST");
    CHECK(lakeMapAgrees(s, r));
    // One plot of water inside the land: a lake, fresh water for the plots beside it (01: Lake).
    s.plot({4, 4}).terrain = coast;
    CHECK(lakeMapAgrees(s, r));
    CHECK(isLake(s, r, {4, 4}));
    CHECK(!isLake(s, r, {5, 4}));
    CHECK(isLakeAdjacent(s, r, {5, 4}));
    CHECK(hasFreshWater(s, r, {5, 4}));
    CHECK(!isLakeAdjacent(s, r, {7, 4}));
    CHECK(!hasFreshWater(s, r, {7, 4}));
    // Nine plots of water are still a lake; a tenth makes them a sea (LAKE_MAX_AREA_SIZE).
    for (int x = 4; x <= 12; ++x) s.plot({x, 10}).terrain = coast;
    CHECK(lakeMapAgrees(s, r));
    CHECK(isLake(s, r, {4, 10}));
    CHECK(hasFreshWater(s, r, {4, 11}));
    s.plot({13, 10}).terrain = coast;
    CHECK(lakeMapAgrees(s, r));
    CHECK(!isLake(s, r, {4, 10}));
    CHECK(!isLake(s, r, {13, 10}));
    CHECK(!isLakeAdjacent(s, r, {4, 11}));
    CHECK(!hasFreshWater(s, r, {4, 11}));
    // Rivers and fresh-water features count too.
    setRiver(s, {2, 13}, Dir::E);
    CHECK(hasFreshWater(s, r, {2, 13}));
    s.plot({10, 2}).feature = r.feature("FEATURE_OASIS");
    CHECK(hasFreshWater(s, r, {11, 2}));
}

TEST(maps_have_lakes) {
    // Lakes in basins (01): the map scripts' lakes, each its own small body of Coast.
    const Rules& r = rules();
    int lakes = 0;
    for (uint64_t seed : {1ull, 2ull, 3ull, 4ull}) {
        std::string err;
        auto g = Game::create(r, sovtest::duelSetup(seed), &err);
        REQUIRE(g);
        const GameState& s = g->state();
        CHECK(lakeMapAgrees(s, r));
        for (int i = 0; i < s.grid.size(); ++i) {
            if (!isLake(s, r, s.grid.at(i))) continue;
            ++lakes;
            CHECK_EQ(s.plot(s.grid.at(i)).terrain, r.terrain("TERRAIN_COAST"));
        }
    }
    CHECK(lakes >= 4);
}

TEST(maps_have_volcanoes_and_geothermal_fissures) {
    // 01, 09 [GS]: Volcanoes on Mountains, Geothermal Fissures on open land; Sovereign density: one Volcano per
    // MAPGEN_MOUNTAINS_PER_VOLCANO Mountains, one Fissure per MAPGEN_LAND_PER_FISSURE land plots, 4 apart (four Duel
    // maps and two Standard ones).
    const Rules& r = rules();
    const TypeIndex volcano = r.feature("FEATURE_VOLCANO"), fissure = r.feature("FEATURE_GEOTHERMAL_FISSURE");
    for (uint64_t seed : {1ull, 2ull, 3ull, 4ull, 5ull, 6ull}) {
        std::string err;
        GameSetup setup = sovtest::duelSetup(seed);
        if (seed > 4) setup.mapSize = "MAPSIZE_STANDARD";  // more of each, closer together
        auto g = Game::create(r, setup, &err);
        REQUIRE(g);
        const GameState& s = g->state();
        int land = 0, mountains = 0;
        std::vector<Hex> volcanoes, fissures;
        for (int i = 0; i < s.grid.size(); ++i) {
            const Plot& p = s.plots[static_cast<size_t>(i)];
            const TerrainType& t = r.terrains[static_cast<size_t>(p.terrain)];
            land += t.water ? 0 : 1;
            mountains += t.relief == Relief::Mountain ? 1 : 0;
            if (p.feature == volcano) {
                CHECK(t.relief == Relief::Mountain);
                volcanoes.push_back(s.grid.at(i));
            }
            if (p.feature == fissure) {
                CHECK(!t.water && t.relief != Relief::Mountain);
                fissures.push_back(s.grid.at(i));
            }
        }
        CHECK_EQ(static_cast<int>(volcanoes.size()), mountains / r.globalInt("MAPGEN_MOUNTAINS_PER_VOLCANO"));
        CHECK_EQ(static_cast<int>(fissures.size()), land / r.globalInt("MAPGEN_LAND_PER_FISSURE"));
        CHECK(!volcanoes.empty() && !fissures.empty());
        for (const std::vector<Hex>* list : {&volcanoes, &fissures}) {
            for (size_t a = 0; a < list->size(); ++a) {
                for (size_t b = a + 1; b < list->size(); ++b) CHECK(s.grid.distance((*list)[a], (*list)[b]) >= 4);
            }
        }
    }
}
