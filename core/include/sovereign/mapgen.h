// Map generation (01-map-and-terrain.md, Map generation). Uses only the
// MapGen RNG stream, so the same seed and rules always give the same map.
#pragma once

#include <string>
#include <vector>

#include "sovereign/rules.h"
#include "sovereign/state.h"

namespace sov {

// Fills state.plots for state.grid: relief, climate, rivers, features,
// resources and continent ids.
void generateMap(GameState& state, const Rules& rules);

// Picks one start plot per player, spaced START_DISTANCE_MAJOR_CIVILIZATION
// apart when the map allows (the spacing relaxes until everyone fits).
bool chooseStartPositions(GameState& state, const Rules& rules, std::string* error);

// True if a land unit can stand on this plot.
bool isLandPassable(const GameState& state, const Rules& rules, Hex h);

// True if a river runs along the edge between h and its neighbour in d.
bool hasRiver(const GameState& state, Hex h, Dir d);
void setRiver(GameState& state, Hex h, Dir d);
bool isRiverAdjacent(const GameState& state, Hex h);

}  // namespace sov
