# Record: the overview spec audited against the core, part 2

Status: part 2 done, 2026-10-19. Previous: `2026-10-10-overview-audit.md`. The game-speed duration scalings of specs/civ6/00-overview.md (Game speed: "Durations measured in turns ... use two softer scalings") against the core, which scaled every timed effect by the full cost percent. Asked by James.

## Fixes

- **Durations in turns take the Half scaling** (00: Game speed). Sovereign rule, since the spec does not say which effect uses which scaling: every duration in turns (timed effects, cooldowns, intervals) uses *Half*; *Slight* stays unused. Costs (gold, production, science, culture, faith, points) keep the full cost percent. `GameSpeedType::durationPercent`, loaded from `durationPercent` in setup.json's `gameSpeeds` (Online 66, Quick 87, Standard 100, Epic 125, Marathon 200; Short Reign 50 is Sovereign's own, as Civ has no such speed; a speed without it falls back to its cost percent), and one helper, `Game::speedTurns(standardTurns)` (rounded down, at least 1 for a positive input). Sites switched:
  - trade route minimum length, `tradeRouteLength` (trade.cpp; its file-local `speedPercent` went with it);
  - the level-2 Research alliance's Eureka period, `allianceEurekas` (research.cpp);
  - Gain Sources' duration, `processSpies` (espionage.cpp);
  - the World Congress's spacing between sessions (worldcongress.cpp);
  - a spy's travel to a new city and its mission turns, `SpyMission` (game.cpp);
  - Levy Military's duration (game.cpp);
  - a bribed or hired barbarian clan camp's turns, `applyClan` (barbarians.cpp).

## Matches

- Kept at the cost percent, as costs: great person costs and scaled activation yields, district, building, project, unit and plot costs, growth and border-growth thresholds, faith purchases, harvest yields, upgrade gold, gear and Benevolence gold, clan bribe/incite/hire gold, goody-hut stockpiles, tech and civic costs.
- The game era's minimum and maximum turns (`processEras`) stay at the cost percent: they pace eras against the game's own length (Marathon's 1500 turns are the cost percent's 300%), not a timed effect.
- The lookup rows in specs/civ6/data/game-setup.md "Game speed durations" do not follow the Half percents (Online 10 -> 8, not 6; Epic 5 -> 10, not 6; Marathon 5 -> 15, not 10; Marathon 30 -> 60 and 60 -> 120 do match). Per James's decision the core uses the percents; the rows look like Civ's rounded per-duration lookups and are left as generated.

## Left for later

- The barbarian raid throttle per difficulty (`BARBARIAN_LOWER_THROTTLE_PER_DIFFICULTY`) and the score's era-buildings and converted-citizens lines stay unbuilt: their engine meaning has no source.

## Tests

`a_levy_lasts_by_the_half_duration_scaling` (test_citystates.cpp), `congress_sessions_are_spaced_by_the_half_duration_scaling` (test_worldcongress.cpp), `durations_take_the_half_scaling_and_costs_the_cost_percent` (test_eras.cpp), the duration percents in `rules_load_known_values` (test_rules.cpp); `a_research_alliance_shares_a_eureka_every_30_turns` now expects Online's 19 turns.

## Results

- GCC and Clang builds clean with -Werror; ctest 8/8 on both (864 unit tests).
- `sovsim --rules data/rules --seed 2026 --players 4 --size MAPSIZE_TINY --turns 100`: turn 101, 25 cities, 103 units, rules c295ae64f0dea381, state 95670d6475eac1ad, the same on GCC and Clang.
- A Marathon AI game (same seed, `--ai --speed GAMESPEED_MARATHON --clans`, 300 turns) ends with no error.
- The 30-turn golden game's state hash file regenerated (rules checksum only; its state hash is unchanged on Standard). No save version bump: the save layout did not change.
