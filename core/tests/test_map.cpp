#include <algorithm>

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
    const auto& ps = g->state().players;
    for (size_t i = 0; i < ps.size(); ++i) {
        CHECK(isLandPassable(g->state(), r, ps[i].startPos));
        for (size_t j = i + 1; j < ps.size(); ++j)
            CHECK(g->state().grid.distance(ps[i].startPos, ps[j].startPos) > r.globalInt("CITY_MIN_RANGE"));
    }
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
