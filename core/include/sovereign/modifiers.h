// Evaluating Civ VI-style modifiers against game state. A modifier applies
// when its source is present (a building in the city, the player's civ, a
// slotted policy, the current government, or everyone), its collection covers the subject, and both requirement sets
// hold (00-overview.md, "Modifier system sketch").
#pragma once

#include <optional>

#include "sovereign/rules.h"
#include "sovereign/state.h"

namespace sov {

struct ReqContext {
    const GameState* state = nullptr;
    const Rules* rules = nullptr;
    const Player* player = nullptr;
    const City* city = nullptr;
    const Plot* plot = nullptr;
};

bool testRequirements(const RequirementSet& set, const ReqContext& ctx);

// Total amount of every city-level modifier with this effect applying to the
// city; `yield` filters yield effects.
Fixed sumCityModifiers(const GameState& s, const Rules& r, const City& city, ModEffect effect,
                       std::optional<YieldType> yield = std::nullopt);

// Total flat plot-yield modifiers for a plot worked by this city.
Fixed sumPlotModifiers(const GameState& s, const Rules& r, const City& city, Hex plot, YieldType yield);

// Percentage bonus to production toward this unit type in the city.
Fixed sumUnitProductionPercent(const GameState& s, const Rules& r, const City& city, TypeIndex unitType);

// Total of a player-wide modifier effect (collection PLAYER).
Fixed sumPlayerModifiers(const GameState& s, const Rules& r, const Player& player, ModEffect effect);

}  // namespace sov
