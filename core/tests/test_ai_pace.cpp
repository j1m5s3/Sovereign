// The AI's pace floor (strategy AI plan, milestone 1): all-AI Duel games must reach a
// minimum of cities, techs and science by turn 150, so economic changes cannot silently
// slow the AI down. `sovsim --bench` prints the full table.
#include "helpers.h"
#include "sovereign/ai.h"

using namespace sov;
using sovtest::rules;

TEST(ai_keeps_pace_on_a_duel_map) {
    ai::PaceSample sum;
    const int games = 3;
    for (int seed = 1; seed <= games; ++seed) {
        GameSetup setup;
        setup.seed = static_cast<uint64_t>(seed);
        setup.mapSize = "MAPSIZE_DUEL";
        setup.disasterIntensity = -1;
        for (int i = 0; i < 2; ++i) setup.players.push_back({rules().civs[static_cast<size_t>(i)].id, false});
        std::string err;
        auto g = Game::create(rules(), setup, &err);
        REQUIRE(g);
        while (g->state().turn < 150 && !g->gameOver()) ai::playTurn(*g);
        const ai::PaceSample p = ai::measurePace(*g);
        sum.cities += p.cities;
        sum.techs += p.techs;
        sum.civics += p.civics;
        sum.science += p.science;
    }
    // Averages x100 per civ. Measured 2026-10-05: 6.8 cities, 19 techs, 12.5 civics, 29 science.
    CHECK(sum.cities / games >= 550);
    CHECK(sum.techs / games >= 1700);
    CHECK(sum.civics / games >= 1100);
    CHECK(sum.science / games >= 2400);
}
