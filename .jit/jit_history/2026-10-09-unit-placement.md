# Record: new units appear in their city, start units nearest the start, levied units on plots of their own (rules fix)

Status: done, 2026-10-09. Found by Claude: the AI soak test on main found a levied archer standing on its city-state's Builder (`2026-10-09-war-targets.md`, "Left open"), and a look at where the core places units found the rest.

## What was wrong

- **New units appeared beside their city.** `unitSpawnPlot` put a unit trained or bought in a city (and every other unit it places: great people, Apostles, Traders back from a route, and the units wonders, villages, pantheons and dedications grant) on the first free plot of `HexGrid::within(city, 1)`. That lists plots row by row from the north, so a free plot north of the city came before the city itself, against its comment "the center comes first". Builders, Settlers and great people stood outside their city from the turn they appeared.
- **Start units went to the north.** Starting units that do not fit on the start plot (the AI's extra units at Immortal and Deity) took the first free plot of `within(start, 2)`, and a later era's units (game setup) of `within(start, 3)`: up to 2 or 3 plots north of the start with free plots beside it.
- **Levied units shared plots.** Levy Military (08) handed a city-state's military units to its suzerain where they stood: in the city-state's city, or on a plot with the city-state's Builder or support unit, so two players' units shared a plot (05: Stacking). When the levy ended they went back to the city-state where they stood, in the suzerain's cities or among its units too, though `2026-10-06-levies.md` says "home".

## Fix

- `HexGrid::nearestFirst(center, radius)`: within()'s hexes, nearest first (each ring in within()'s order). The starting units, the AI's extra units and a later era's units take the first free plot in that order; `unitSpawnPlot` looks at the city, then at its neighbours in the same order, without building the list.
- `Game::mayStand(unit, plot)`: the unit's kind of ground (a ship: water or its owner's city; a land unit: land, or the water it is embarked on), no other unit of its layer and no other player's unit or city there. `standingPlotNear(unit, around, radius)`: the nearest plot where it may.
- A levied unit that may not stand where it is steps to the nearest plot where it may, within `kLevyPlacement` (5) plots. When the levy ends, each unit goes to the nearest plot it may stand on within 5 plots of the city-state's city (its capital), the city first; with none, or with the city-state gone, it is disbanded. A moved unit loses its order, its escort link and its fortification (`relocateUnit`).

## Sovereign readings

- Where levied units stand is not in the spec: they keep their plots unless they must leave, and go back to their city-state's city when the levy ends.

## Results

- In 16 AI games (Small, 6 AI, seeds 1000-1015, to turn 200; 40 levies on main, 41 here), counted as each owner's turn began: units sharing a plot with another player's unit, 5 on main and none here; units standing in another player's city, 78 on main (levied units in their city-state's city) and none here.
- 128 AI games (Small, 6 AI, turn 200) against main: no change past noise (science +0.5 ± 1.3, culture +1.2 ± 0.8, production +1.2 ± 1.8, cities +0.01 ± 0.07).
- Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.
- Speed: the 150-turn benchmark games (Small, 6 AI, seeds 1-8) take 10.09 s of CPU against 9.97 s on main (+1%), with 419 cities and 1177 units at the end against 431 and 1176. Seed 5 alone is a slower game now (1.37 s against 1.18 s), with more units.
- The golden file's state hash changes; the rules checksum does not.

## Tests

- `a_new_unit_appears_in_its_city`: a Warrior appears in an empty city; with a Warrior there, the next one beside it and a Builder in it.
- `levied_units_stand_alone_and_go_home`: the city-state's garrison and the Warrior on its Builder's plot each step one plot away when levied, the Builder staying; an embarked Warrior alone on the water stays, one embarked on its Builder's plot steps ashore past the water beside it; when the levy ends, one of them far off on its suzerain's Builder, they go back to the city or beside it, on plots of their own.
- `deity_ai_starts_with_extra_units_and_the_human_does_not` and `a_game_can_begin_in_a_later_era`: every plot nearer the start than one of the units holds another of its layer. `hex_nearest_first_lists_the_disc_by_distance`.
- A mutation check of the new lines: 15 mutants, 14 fail the tests; the one left changes nothing (a levied unit looking for a plot without first checking its own: the search finds its own first). Not run: the starting units' own loop back in within() order, which changes nothing while the Settler and Warrior stand on the start plot.

## Left open

- Barbarian camps still release their units on the first free plot of `within(camp, 1)`, north of the camp before the camp itself; units spawning on the camp would guard it, a change to barbarian play left for its own look.
