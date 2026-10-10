# Record: the eras and climate spec audited again against the core

Status: parts 1 to 3 done, 2026-10-10. Previous: `2026-10-10-religion-audit-2.md`. A second read of specs/civ6/09-civs-eras-victory-climate.md (the first passes are `2026-10-09-climate-audit.md` and `2026-10-09-eras-audit.md`) against the core. The era countdown, thresholds, Heroic Age, dedication windows, moment windows, CO2, phases, droughts, floods and spacing match. Chosen by Claude under James's standing consent.

## Fixes (part 1)

- **Hurricanes pillage districts on coastal lowlands** at the lowland rate (100% for a Category 4). `Game::lowlandBand` left out plots under a city, district or wonder, so the earlier hurricane fix never reached districts; `lowlandBand(plot, true)` now counts them for storm damage, while the sea still never takes a built plot.
- **A city-state's first suzerain earns the moment however the envoys came**: first meetings, quests, great people and the ruler's visits as well as sent envoys, through `Game::noteFirstSuzerain`. Envoys short of suzerainty claim nothing.
- **The world (game) era sets the minimum trade route length** (04: World era; 09), `GameState::gameEra`, not the era most civs have entered (`Game::worldEra`, kept for great people as 07 says).

## Fixes (part 2)

- **Coastal lowlands are flagged once, when the game begins** (09: "flagged at map generation"), in `Plot::lowland` by `Game::flagLowlands` (`GameState::lowlandsFlagged`, save version 100). They were worked out again on every call, so after a plot drowned the land behind it, now on the sea, could join the bands and drown in turn. A plot built on keeps its band: it still counts toward its city's Flood Barrier cost and storms' lowland damage, and the sea floods it but never takes it (cities, districts and wonders stay land, a Sovereign reading the spec does not cover).

## Fixes (part 3)

- **Burnt woods grow back** (09: "burn Woods/Rainforest (9 turns) ... then regrow with +1 yield"). A fire turned woods straight into Burnt Woods for ever, so the data's regrowth row (+1 Production) never applied and every fire raised deforestation, and with it CO2's effect, for good. Now a struck plot burns for the fire's duration (Burning Woods or Rainforest), lies burnt as long again (+1 Food), then its woods or rainforest grow back (+1 Production), `Plot::regrow` counting down in `Game::processClimate` (save version 101). How long the plot lies burnt is a Sovereign reading; the data gives no time. Burning and burnt plots count as woods for deforestation, since they grow back.

## Left for later parts

- Heavy and Hyperreal add +1 range to every disaster; the data gives an "Extra range %" for the stronger eruptions only (spec and data differ).
- A disaster's population loss falls only on a struck City Center (engine rule unknown).
- Sky and Stars and Automaton Warfare's resource bonuses (deferred in `2026-10-05-dedications.md`); Heartbeat of Steam's Information-era cap is moot, no Future-era wonder exists.

## Results

- 128 AI games (Small, 6 AI, turn 200) against the religion audit: every figure within one standard error.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant caught (one did not build).

## Tests

Part 3: `burnt_woods_grow_back` (test_climate.cpp; `disasters_strike_on_their_own_at_high_intensity` now looks for any fire's mark); every mutant caught; the 8 soak games clean; 128 AI pace games: Production +3.7 (t +3.0) and science +1.9 at turn 200 from the woods growing back, the rest within noise.

Part 2: `the_sea_takes_only_the_lowlands_flagged_at_the_start` and `built_lowlands_still_count_toward_a_flood_barrier` (test_climate.cpp); every mutant caught; 128 AI pace games identical; the 8 soak games clean. Part 1: `a_hurricane_pillages_every_lowland_district` (test_climate.cpp), `a_quest_envoy_can_make_the_first_suzerain` and `the_first_suzerain_moment_waits_for_a_suzerain` (test_quests.cpp), `the_world_era_sets_the_trade_route_minimum` (test_trade.cpp). Each fails with its fix undone (mutation).
