# Record: the eras and climate spec audited again against the core

Status: part 1 done, 2026-10-10. Previous: `2026-10-10-religion-audit-2.md`. A second read of specs/civ6/09-civs-eras-victory-climate.md (the first passes are `2026-10-09-climate-audit.md` and `2026-10-09-eras-audit.md`) against the core. The era countdown, thresholds, Heroic Age, dedication windows, moment windows, CO2, phases, droughts, floods and spacing match. Chosen by Claude under James's standing consent.

## Fixes (part 1)

- **Hurricanes pillage districts on coastal lowlands** at the lowland rate (100% for a Category 4). `Game::lowlandBand` left out plots under a city, district or wonder, so the earlier hurricane fix never reached districts; `lowlandBand(plot, true)` now counts them for storm damage, while the sea still never takes a built plot.
- **A city-state's first suzerain earns the moment however the envoys came**: first meetings, quests, great people and the ruler's visits as well as sent envoys, through `Game::noteFirstSuzerain`. Envoys short of suzerainty claim nothing.
- **The world (game) era sets the minimum trade route length** (04: World era; 09), `GameState::gameEra`, not the era most civs have entered (`Game::worldEra`, kept for great people as 07 says).

## Left for later parts

- Coastal lowlands are worked out on every call, so the band can creep inland after a plot drowns, and a plot built on leaves the band (and the Flood Barrier's cost) for good. Fix: fix the band at map generation in the plot (save version).
- Forest fires burn straight to Burnt Woods and never regrow (the data's +1 Production regrowth unused); burnt plots count as lost woods for deforestation for ever.
- Heavy and Hyperreal add +1 range to every disaster; the data gives an "Extra range %" for the stronger eruptions only (spec and data differ).
- A disaster's population loss falls only on a struck City Center (engine rule unknown).
- Sky and Stars and Automaton Warfare's resource bonuses (deferred in `2026-10-05-dedications.md`); Heartbeat of Steam's Information-era cap is moot, no Future-era wonder exists.

## Results

- 128 AI games (Small, 6 AI, turn 200) against the religion audit: every figure within one standard error.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant caught (one did not build).

## Tests

`a_hurricane_pillages_every_lowland_district` (test_climate.cpp), `a_quest_envoy_can_make_the_first_suzerain` and `the_first_suzerain_moment_waits_for_a_suzerain` (test_quests.cpp), `the_world_era_sets_the_trade_route_minimum` (test_trade.cpp). Each fails with its fix undone (mutation).
