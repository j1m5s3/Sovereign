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

// Religion in a city (06: Spread mechanics). Followers of a religion are the population times
// its share of the pressure, with unbelief pressing RELIGION_SPREAD_ATHEISM_PRESSURE_PER_POP
// per citizen; the majority religion is followed by more than half (-1: none).
SOV_API int religionFollowers(const GameState& s, const Rules& r, const City& city, int religion);
SOV_API int majorityReligion(const GameState& s, const Rules& r, const City& city);
SOV_API bool religionHas(const GameState& s, int religion, TypeIndex belief);

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

// Policy cards (04: Policies): production toward a building, district or project in the city; great
// person points of a class from the city and from the player; extra yields on a trade route of the
// player's (`ally`: to an ally; `cityState`: to a city-state; `suzerain`: one the player is suzerain of).
SOV_API Fixed sumItemProductionPercent(const GameState& s, const Rules& r, const City& city, ProductionItem item);
SOV_API Fixed sumCityGreatPersonPoints(const GameState& s, const Rules& r, const City& city, TypeIndex gpClass);
SOV_API Fixed sumPlayerGreatPersonPoints(const GameState& s, const Rules& r, const Player& player, TypeIndex gpClass);
SOV_API Yields tradeRouteModifierYields(const GameState& s, const Rules& r, const Player& owner, bool domestic, bool ally, bool cityState,
                                        bool suzerain, bool toDestination = false);

// Tourism from each of the player's completed districts of this type (Masaru Ibuka, Jamsetji Tata).
SOV_API int districtTourism(const GameState& s, const Rules& r, const Player& player, TypeIndex district);

// Combat XP bonus percent for the player's units of this class.
SOV_API Fixed sumUnitXpPercent(const GameState& s, const Rules& r, const Player& player, const std::string& unitClass);

}  // namespace sov
