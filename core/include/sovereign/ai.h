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

#include "sovereign/game.h"

namespace sov::ai {

// Plays the current player's whole turn and ends it (does nothing once the game is won).
void playTurn(Game& game);

// Value of founding a city on this plot for the player (higher is better);
// negative when a city cannot be founded there.
int settleScore(const Game& game, PlayerId player, Hex plot);

// Sum of combat strength (scaled by health) of the player's land military units.
int militaryStrength(const Game& game, PlayerId player);

}  // namespace sov::ai
