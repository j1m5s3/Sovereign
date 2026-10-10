# Record: the districts, buildings and wonders spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-10-climate-audit-2.md`. A second read of specs/civ6/03-districts-buildings-wonders.md (the first is `2026-10-09-districts-audit.md`) against the core found five rules off. Adjacency, placement, district costs, building prerequisites, power, regional buildings, Housing, Amenities and the other wonder effects match. Chosen by Claude under James's standing consent.

## Fixes

- **A wonder's free units come in the numbers its data gives**: Kotoku-in 4 Warrior Monks, Mahabodhi Temple 2 Apostles, Meenakshi Temple 2 Gurus, Statue of Zeus 3 Spearmen, 3 Archers and a Battering Ram. `Game::completeWonder` made one of each and ignored the count; the units now take the nearest free plots out to 3 from the city (`Game::unitSpawnPlot(city, type, radius)`).
- **Kilwa Kisiwani grants its 3 Envoys** (data: "grants 3 Envoy(s) (one-time)"). The line sat among the wonder's code effects but nothing granted it; `tools/rules_gen/gen_rules.py` now makes it an `ENVOYS` effect (`data/rules/wonders.json` regenerated). Rogue State still blocks it.
- **Only a city with coastal lowlands builds a Flood Barrier** (03, 09: it guards a city's lowlands, its cost scaled by them). Any inland city could build one for 80 Production and earn the coastal-flood moment; `Game::cityLowlands` now gates it.
- **The Oracle's +2 from a Theater Square are Great Writer points only** (data: Oracle), not Writer, Artist and Musician points each. It goes to each district's first class.
- **The Terracotta Army promotes land units only** (03: "all current land units"); ships and aircraft no longer gain a level.

## Left open

- Spec lines that differ from the data, the data followed: the Seastead's Housing (2, data "4 per 2 tiles"), the Shipyard's +1 Food (on the building), the Mausoleum on the sea's Coast only.
- Great people's unit effects carry no count above 1 in the data, so their path still makes one.

## Results

- 128 AI games (Small, 6 AI, turn 200) against the climate audit: every figure within one and a half standard errors.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant caught.
- The 30-turn golden game's state hash file regenerated (rules changed).

## Tests

`wonders_grant_every_unit_and_envoy_they_promise`, `the_oracle_gives_a_theater_square_writer_points`, `the_terracotta_army_promotes_land_units` (test_wonders.cpp), `only_a_city_with_lowlands_builds_a_flood_barrier` (test_climate.cpp).
