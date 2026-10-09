// Map generation (01-map-and-terrain.md, Map generation). Uses only the
// MapGen RNG stream, so the same seed and rules always give the same map.
#pragma once

#include "sovereign/api.h"

#include <string>
#include <vector>

#include "sovereign/rules.h"
#include "sovereign/state.h"

namespace sov {

// Fills state.plots for state.grid: relief, climate, rivers, features,
// resources and continent ids.
SOV_API void generateMap(GameState& state, const Rules& rules);

// Picks one start plot per player, spaced START_DISTANCE_MAJOR_CIVILIZATION
// apart when the map allows (the spacing relaxes until everyone fits): the
// best-scoring plots, handed to the players in a shuffled order.
SOV_API bool chooseStartPositions(GameState& state, const Rules& rules, std::string* error);
// How good a start h is: the food (x3), production (x2) and gold of the plots
// within 2, 2 for each passable land plot within 3, 15 for fresh water, 6 for
// a coast.
SOV_API int startScore(const GameState& state, const Rules& rules, Hex h);

// The least a major civ's start has on the six plots beside it (01, Map generation, step 7: "guarantee minimum
// food/production"; the base game's start script, __AddBonusFoodProduction): Food in all and on the best plot, and
// Production the same.
constexpr int kStartFood = 7, kStartBestFood = 3, kStartProduction = 5, kStartBestProduction = 2;
// For each major civ's start short of them, one Bonus resource that gives Food, and one that gives Production, on a
// plot beside it that has none and fits it.
SOV_API void addStartBonuses(GameState& state, const Rules& rules);

// True if a land unit can stand on this plot.
SOV_API bool isLandPassable(const GameState& state, const Rules& rules, Hex h);
// The same for a plot already in hand.
inline bool isLandPassable(const Rules& rules, const Plot& p) {
    const TerrainType& t = rules.terrains[static_cast<size_t>(p.terrain)];
    if (t.water) return false;
    // A Mountain Tunnel [GS] opens its mountain (01: Mountain tunnels).
    if (t.impassable && !(p.improvement != kNone && rules.improvements[static_cast<size_t>(p.improvement)].tunnel)) return false;
    return p.feature == kNone || !rules.features[static_cast<size_t>(p.feature)].impassable;
}
// Places the map size's natural wonders on clusters of valid plots away from every start (01).
SOV_API void placeNaturalWonders(GameState& state, const Rules& rules);
// Scatters tribal villages over open land away from every start (01: Tribal Villages).
SOV_API void placeVillages(GameState& state, const Rules& rules);

// True if a river runs along the edge between h and its neighbour in d.
SOV_API bool hasRiver(const GameState& state, Hex h, Dir d);
SOV_API void setRiver(GameState& state, Hex h, Dir d);
SOV_API bool isRiverAdjacent(const GameState& state, Hex h);

// Lakes (01: Lake): water in a body of at most LAKE_MAX_AREA_SIZE plots. They are Coast
// terrain; the size of the body they belong to tells them apart from the sea. Given `lakes`,
// a lakeMap of this state, these read it instead of measuring the water around h.
SOV_API bool isLake(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes = nullptr);
SOV_API bool isLakeAdjacent(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes = nullptr);  // a lake plot beside h
// isLake for every plot, by plot index, from one look at each body of water.
SOV_API std::vector<uint8_t> lakeMap(const GameState& state, const Rules& rules);
// Fresh water for a city or district on h (01): a river along it, or a lake or a fresh-water
// feature (Oasis, some natural wonders) beside it.
SOV_API bool hasFreshWater(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes = nullptr);

}  // namespace sov
