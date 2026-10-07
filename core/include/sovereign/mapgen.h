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
// apart when the map allows (the spacing relaxes until everyone fits).
SOV_API bool chooseStartPositions(GameState& state, const Rules& rules, std::string* error);

// True if a land unit can stand on this plot.
SOV_API bool isLandPassable(const GameState& state, const Rules& rules, Hex h);
// Places the map size's natural wonders on clusters of valid plots away from every start (01).
SOV_API void placeNaturalWonders(GameState& state, const Rules& rules);
// Scatters tribal villages over open land away from every start (01: Tribal Villages).
SOV_API void placeVillages(GameState& state, const Rules& rules);

// True if a river runs along the edge between h and its neighbour in d.
SOV_API bool hasRiver(const GameState& state, Hex h, Dir d);
SOV_API void setRiver(GameState& state, Hex h, Dir d);
SOV_API bool isRiverAdjacent(const GameState& state, Hex h);

// Lakes (01: Lake): water in a body of at most LAKE_MAX_AREA_SIZE plots. They are Coast
// terrain; the size of the body they belong to tells them apart from the sea.
SOV_API bool isLake(const GameState& state, const Rules& rules, Hex h);
SOV_API bool isLakeAdjacent(const GameState& state, const Rules& rules, Hex h);  // a lake plot beside h
// Fresh water for a city or district on h (01): a river along it, or a lake or a fresh-water
// feature (Oasis, some natural wonders) beside it.
SOV_API bool hasFreshWater(const GameState& state, const Rules& rules, Hex h);

}  // namespace sov
