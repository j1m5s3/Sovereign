// The computer opponent (MVP-6; 10-ai-ui-implementation.md, "Recommended clone
// architecture"). It plays through public queries and Game::submit only, like a
// human, so every decision lands in the command log and replays exactly.
//
// Layers, run in order each turn:
//   1. Diplomacy: war on a weaker neighbour, peace when losing or tired of war.
//   2. Economy: research and civics by unlock value, government and policies,
//      production by need (garrison, settlers, builders, army) then yield value.
//   3. Military: attacks by preview odds (ranged first, then melee to finish),
//      garrisons, retreat to heal, armies toward the target city, barbarian camps.
//   4. Civilians: settlers to scored sites (StandardSettlePlot weights), builders
//      to the best unimproved plots.
// It is deterministic: the same state always gives the same commands.
#pragma once

#include "sovereign/api.h"

#include "sovereign/game.h"

namespace sov::ai {

// Plays the current player's whole turn and ends it (does nothing once the game is won).
SOV_API void playTurn(Game& game);

// Value of founding a city on this plot for the player (higher is better);
// negative when a city cannot be founded there.
SOV_API int settleScore(const Game& game, PlayerId player, Hex plot);

// Sum of combat strength (scaled by health) of the player's land military units.
SOV_API int militaryStrength(const Game& game, PlayerId player);

// Grand strategies (10-ai-ui-implementation.md, Strategies): re-evaluated from the state every
// turn; at most one victory strategy is active, the situational ones stack.
enum class Strategy : uint8_t {
    ScienceVictory = 0, CultureVictory, ReligiousVictory, DominationVictory, DiplomaticVictory,
    RapidExpansion, Naval, WonderObsessed, DarkAge,
    Count
};
SOV_API std::vector<Strategy> strategies(const Game& game, PlayerId player);
SOV_API const char* strategyName(Strategy s);

// How far along the major civs are, averaged over the living ones (the pace benchmark,
// `sovsim --bench`; values x100 so averages keep two decimals).
struct PaceSample {
    int turn = 0;
    int64_t cities = 0, population = 0, techs = 0, civics = 0, era = 0;
    int64_t science = 0, culture = 0, production = 0, gold = 0;
};
SOV_API PaceSample measurePace(const Game& game);

}  // namespace sov::ai
