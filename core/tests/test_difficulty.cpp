// Difficulty levels (00-overview.md, Difficulty levels; leader doc §10, Difficulty [decided]):
// skill first; Civ's AI bonuses only at Immortal and Deity; human bonuses at Settler and Chieftain.
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
int unitsOf(const Game& g, PlayerId p, const char* type) {
    int n = 0;
    for (const Unit& u : g.state().units) n += u.owner == p && u.type == rules().unit(type) ? 1 : 0;
    return n;
}

std::unique_ptr<Game> start(int difficulty) {
    GameSetup setup;
    setup.seed = 3;
    setup.mapSize = "MAPSIZE_DUEL";
    setup.difficulty = difficulty;
    setup.players.push_back({rules().civs[0].id, true});
    setup.players.push_back({rules().civs[1].id, false});
    std::string err;
    return Game::create(rules(), setup, &err);
}
}  // namespace

TEST(difficulty_levels_from_setup) {
    const Rules& r = rules();
    REQUIRE(r.difficulties.size() == 8u);
    CHECK_EQ(r.difficulties[3].name, std::string("Prince"));
    CHECK_EQ(r.difficulties[7].aiYieldPercent, 32);
    CHECK_EQ(r.difficulties[5].aiYieldPercent, 0);  // Sovereign: no AI bonuses below Immortal
    CHECK_EQ(r.difficulties[0].humanCombat, 3);
}

TEST(deity_ai_starts_with_extra_units_and_the_human_does_not) {
    auto prince = start(3);
    auto deity = start(7);
    REQUIRE(prince);
    REQUIRE(deity);
    CHECK_EQ(unitsOf(*prince, 1, "UNIT_SETTLER"), 1);
    CHECK_EQ(unitsOf(*deity, 1, "UNIT_SETTLER"), 3);
    CHECK_EQ(unitsOf(*deity, 1, "UNIT_WARRIOR"), 5);
    CHECK_EQ(unitsOf(*deity, 1, "UNIT_BUILDER"), 2);
    CHECK_EQ(unitsOf(*deity, 0, "UNIT_SETTLER"), 1);  // the human: the normal start
    // They stand as near the start as the land allows: every plot nearer it than one of them holds another of its layer.
    const GameState& s = deity->state();
    const Hex home = s.players[1].startPos;
    for (const Unit& u : s.units) {
        const UnitLayer layer = rules().units[static_cast<size_t>(u.type)].layer;
        if (u.owner != 1 || layer == UnitLayer::Leader) continue;
        for (const Hex& h : s.grid.within(home, s.grid.distance(u.pos, home) - 1)) {
            if (isLandPassable(s, rules(), h)) CHECK(s.unitAt(h, layer, rules()) != nullptr);
        }
    }
}

TEST(top_difficulty_ai_cities_yield_more) {
    GameState s = flatState(20, 14, 2);
    s.players[0].human = true;
    addCity(s, 0, {4, 6}, true, 4);
    addCity(s, 1, {14, 6}, true, 4);
    s.setup.difficulty = 7;
    auto g = Game::fromScenario(rules(), s);
    const auto sci = [&](const Game& game, CityId id) { return game.cityReport(id).yields[static_cast<size_t>(YieldType::Science)]; };
    s.setup.difficulty = 3;
    auto prince = Game::fromScenario(rules(), std::move(s));
    const CityId human = g->state().cities[0].id, ai = g->state().cities[1].id;
    CHECK(sci(*g, ai) == sci(*prince, ai) * 132 / 100);
    CHECK(sci(*g, human) == sci(*prince, human));
}

TEST(low_difficulty_helps_the_human_in_combat_and_slows_the_ai) {
    GameState s = flatState(20, 14, 2);
    s.players[0].human = true;
    const UnitId mine = addUnit(s, "UNIT_WARRIOR", 0, {6, 6});
    const UnitId theirs = addUnit(s, "UNIT_WARRIOR", 1, {7, 6});
    s.setup.difficulty = 0;
    auto g = Game::fromScenario(rules(), s);
    const Unit& a = *g->state().unit(mine);
    const Unit& b = *g->state().unit(theirs);
    CHECK_EQ(g->combatStrength(a, b, true, false), g->combatStrength(b, a, true, false) + 3);
    // Warlord and below: no Rapid Expansion (Strategies data).
    GameState s2 = flatState(30, 20, 1);
    addCity(s2, 0, {6, 6}, true, 3);
    s2.players[0].visibility.assign(static_cast<size_t>(s2.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    s2.setup.difficulty = 2;
    auto warlord = Game::fromScenario(rules(), s2);
    const std::vector<ai::Strategy> on = ai::strategies(*warlord, 0);
    CHECK(std::find(on.begin(), on.end(), ai::Strategy::RapidExpansion) == on.end());
    s2.setup.difficulty = 3;
    auto prince = Game::fromScenario(rules(), std::move(s2));
    const std::vector<ai::Strategy> on2 = ai::strategies(*prince, 0);
    CHECK(std::find(on2.begin(), on2.end(), ai::Strategy::RapidExpansion) != on2.end());
}

TEST(difficulty_survives_a_save) {
    auto g = start(6);
    REQUIRE(g);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().setup.difficulty, 6);
    CHECK_EQ(loaded->difficulty().name, std::string("Immortal"));
}
