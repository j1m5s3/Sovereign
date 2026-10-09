# Record: a poor start gets a Food and a Production resource beside it (rules gap)

Status: done, 2026-10-09. Previous: `2026-10-09-fair-starts.md`, whose "Left open" names the spec's start guarantee as not modelled. Chosen by Claude under James's standing consent.

## What was missing

- 01 (Map generation, step 7) says start positions "guarantee minimum food/production", with no numbers; the core never did. In 64 AI games (Small, 6 AI), 156 of the 384 major civs' starts fell short on Food and 114 on Production by the base game's measure below.

## Fix

- `addStartBonuses` (mapgen.cpp), run once the starts are handed out: for each major civ, the Food and Production on the six plots beside its start (terrain, feature and resource), in all and on the best plot. Under 7 Food in all or no plot with 3, it places one Bonus resource that gives Food on a plot beside the start; under 5 Production in all or no plot with 2, one that gives Production.
- The plot: from a random one of the six, in turn round the start, the first with no resource that one of them fits (on a featured plot the feature decides, else the terrain, as the map's resources are placed), the list tried in a fresh random order at each plot. The map generator's random numbers (`RngStream::MapGen`).
- The thresholds and the way the plot is picked are the base game's start script (`__AddBonusFoodProduction`, `__AddFood`, `__AddProduction` in AssignStartingPlots.lua), read from a copy at https://github.com/bszonye/civ6-capslock/blob/master/assignstartingplots.lua. There, Food and Production resources are those tagged CLASS_FOOD and CLASS_PRODUCTION; here, the Bonus resources whose yields have Food (Bananas, Cattle, Fish, Rice, Sheep, Wheat) or Production (Deer, Stone).

## Sovereign readings

- A Food or Production resource is read from the resource's yields, not the base game's tags.

## Results

- 64 AI games (Small, 6 AI, seeds 1-64): starts short of Food 156 of 384 before, 99 after; short of Production 114 before, 87 after (one resource cannot always close the gap, and on desert or plains no Production resource fits).
- How much the start decides is about the same: by turn 150, the civ with the best start (by `startScore`) made 17 more science than its civ and game would suggest (16 before), the two worst 9 less (7 to 8 before); the gap between the best and worst civ in a game was 72 science against 78 (noise of a few points either way). A start's room to expand, not the plots beside it, is most of what sets it apart (inferred: the best start also founds the most cities, 7.9 against 6.8).
- 128 AI games (Small, 6 AI, turn 200) against #241: no change past noise (science -0.3 ± 1.5, production +2.0 ± 1.8, culture +1.1 ± 0.9, population +0.4 ± 0.5, cities -0.03 ± 0.07).
- Speed: only game creation changes.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; 5 setups saved and reloaded at 8 cuts end on the straight game's state.

## Tests

- `map_a_poor_start_gets_food_and_production_beside_it`: eight starts, each resource checked to stand beside the start, and a city-state's start on bare grassland left alone. All grassland gets one of each kind; all plains gets Wheat and no Production resource (none fits); exactly 7 Food with a 3-Food plot and 5 Production with a 2-Production plot gets none; a 3-Food plot with 3 or 6 Food in all gets Sheep; a 2-Production plot with 4 Production in all gets Stone; grassland hills with 5 Production but no 2-Production plot get Stone; wooded grassland gets Deer and no Food resource (the woods decide what fits).
- `ai_soak_takes_a_capital_and_replays` now uses seed 63. The golden file's state hash changes; the rules checksum does not.
- Mutation check: 18 mutants of the new code (each threshold one up and one down, each half of the two conditions, placing on a plot with a resource, placing more than one, placing by terrain only on a featured plot, non-Bonus resources, the best plot's yield, city-states included); the test catches all 18 (the woods and city-state starts were added to catch the last two).

## Left open

- Start quality still decides a lot within a game (above). The spec's other start options (Balanced, Legendary) are map options, not planned.
