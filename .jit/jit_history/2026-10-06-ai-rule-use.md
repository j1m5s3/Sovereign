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

## Follow-up: treasuries

An all-AI soak game (seed 12, 6 civs, Small, 350 turns) ended with the city-state Muscat at -1437 gold. It had taken two cities and kept 22 buildings, and with no trade routes it lost about 10 gold a turn from turn 200. Bankruptcy works as the spec says (Amenities lost, units disbanded), but the AI never reacted. Now:

- a building whose upkeep the gold per turn cannot carry is valued at a quarter, unless it pays at least its upkeep in Gold;
- in debt and still losing gold, the AI locks up to half of each city's citizens onto the plots that pay the most Gold (2+), and frees them once the treasury is above 100.

Muscat now stays between -71 and +69 gold. The pace benchmark is unchanged (7.7 cities, science 58.3 at turn 200).

## Follow-up: loyalty from city-states and Free Cities

The same soak game showed early capitals (Paris, population 1, turn 49; London, turn 89) revolting under pressure from neighbouring city-states, then joining those city-states. Civ VI's loyalty sources (CivFanatics Civ VI Loyalty Guide, civilization.fandom.com Loyalty (Civ6)) say city-states and Free Cities exert no loyalty pressure, and a Free City joins the civilization that pressed it hardest. Capitals can flip in Civ VI, so they still can here. Now city-states' and Free Cities' citizens press on no other city (`loyaltyPressure`), and a Free City joins only a major civ (`processFreeCities`). Pace benchmark at turn 200: 8.4 cities (was 7.7), population 49.7 (43.6), science 65.4 (58.3), production 99.3 (89.7).
