# Note: rules the AI did not use (step 6, "everything else")

Status: done, 2026-10-06. Found by listing the command factories `core/src/ai.cpp` never calls: harvesting, plot purchases, Great Work moves, tunnels, razing, escorts, gear removal, locking plots, queueing (the World Congress votes and deal answers are made inside the core for AI players). Chosen by Claude under James's standing consent.

## Built

- **Plot purchases:** with gold far above its reserve (4× the reserve on top of the price), the AI buys a plot within 3 of a city that holds a luxury or strategic resource it has none of. One plot per city per turn, the cheapest.

## Tried and dropped

All results are from the pace benchmark (`sovsim --bench 4 --players 6 --size MAPSIZE_SMALL --turns 200`), at turn 200. The baseline was 7.6 cities, science 59.5, production 91.4.

- **Harvesting:** Builders chopping woods or harvesting bonus resources into a wonder or district under way.
  - Together with a looser plot rule it gave 7.0 cities, science 48.9, production 82.7.
  - The harvest takes the plot's yield for good. A better rule would weigh what the plot gives over time against the one-time production.
- **Looser plot purchases:** any strategic resource, or a luxury we lack, at 2× the reserve. This gave 7.2 cities and science 53.7: the gold went to plots instead of buildings.

The strict rule kept: 7.7 cities, science 58.2, production 89.4, which is within the noise of 4 games.

## Still unused by the AI

Great Work moves (theming), Mountain Tunnels, razing, gear removal, locked plots, production queues.
