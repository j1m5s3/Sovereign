// Evaluating Civ VI-style modifiers against game state. A modifier applies
// when its source is present (a building in the city, the player's civ, a
// slotted policy, the current government, or everyone), its collection covers the subject, and both requirement sets
// hold (00-overview.md, "Modifier system sketch").
#pragma once

#include "sovereign/api.h"

#include <optional>
#include <string>
#include <vector>

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

SOV_API bool testRequirements(const RequirementSet& set, const ReqContext& ctx);

// Total amount of every city-level modifier with this effect applying to the
// city; `yield` filters yield effects.
Fixed sumCityModifiers(const GameState& s, const Rules& r, const City& city, ModEffect effect,
                       std::optional<YieldType> yield = std::nullopt);

// Total flat plot-yield modifiers for a plot worked by this city.
SOV_API Fixed sumPlotModifiers(const GameState& s, const Rules& r, const City& city, Hex plot, YieldType yield);

// Percentage bonus to production toward this unit type in the city.
SOV_API Fixed sumUnitProductionPercent(const GameState& s, const Rules& r, const City& city, TypeIndex unitType);

// Total of a player-wide modifier effect (collection PLAYER).
SOV_API Fixed sumPlayerModifiers(const GameState& s, const Rules& r, const Player& player, ModEffect effect);

// Abilities the player's modifiers grant (each still filtered by the ability's unit classes).
SOV_API std::vector<TypeIndex> grantedAbilities(const GameState& s, const Rules& r, const Player& player);

// Flat combat strength from the player's modifiers for a unit of this class.
int sumUnitStrength(const GameState& s, const Rules& r, const Player& player, const std::string& unitClass,
                    bool vsBarbarian);

// Adjacency bonus percent for the player's districts of this type (Natural Philosophy...).
SOV_API int sumDistrictAdjacencyPercent(const GameState& s, const Rules& r, const Player& player, TypeIndex district);

// Combat XP bonus percent for the player's units of this class.
SOV_API Fixed sumUnitXpPercent(const GameState& s, const Rules& r, const Player& player, const std::string& unitClass);

}  // namespace sov
