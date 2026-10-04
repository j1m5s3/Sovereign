# Headless rules core, build-plan step 1 (Civ core through MVP-7)

status: IN_PROGRESS · slice: rules-core · base: ba79f37 · created: 2026-10-04 · updated: 2026-10-04 18:05

Build the engine-independent C++ rules core (`core/`) through MVP-7 of `specs/civ6/10-ai-ui-implementation.md` ("Recommended build order"), with tests, as build-plan step 1 of `specs/sovereign/engine-and-architecture.md`. Each milestone is one PR, merged to `main` once CI is green. Out of scope for this slice: the Unreal bridge (step 2, needs James's PC), the leader character (step 3), live scenes, art.

## Decisions (do not reopen)
- Core is plain C++17, no Unreal headers, builds standalone with CMake; namespace `sov`. Enforced by `tools/check_core_rules.py` (no float/double, `<random>`, unordered containers, wall clock, `<cmath>`, Unreal includes), which runs in CTest.
- Numbers: `sov::Fixed`, int64 with 4 decimal places; data numbers are parsed from text, never through a float. 128-bit intermediates via `__int128` (GCC/Clang) or `_mul128/_div128` (MSVC x64).
- RNG: xoshiro256** per stream (MapGen, Gameplay, Combat, AI, Visual), seeded from the game seed via splitmix64; own `below()` with rejection sampling.
- Hex grid: pointy-top, odd-row offset storage (odd rows shifted east), axial math, east-west wrap; direction order NE, E, SE, SW, W, NW. River flags live on each plot's E/SE/SW edges.
- Every state change is a `sov::Command` (flat POD) through `Game::submit`; rejected commands are not logged. `Game::replay(setup, log)` must reproduce the state hash exactly.
- Rules data: JSON in `data/rules/`. `globals.json`, `terrain.json`, `resources.json`, `units.json` are generated from `specs/civ6/data` by `tools/rules_gen/gen_rules.py` (CTest checks they are current); `civilizations.json` (Sovereign's 12 civs) and `setup.json` are hand-written. Extra rules directories layer on top by row id (mods); `"delete": true` removes a row. The rules checksum is stored in saves.
- Saves: magic `SOVS`, `kSaveVersion` (1), rules checksum, state, command log; little-endian explicit widths. Golden hash `core/tests/golden/duel_seed2026_30turns.hash` must match on GCC, Clang and MSVC; regenerate with `SOVEREIGN_UPDATE_GOLDEN=1` only for intended rules/format changes (bump `kSaveVersion` when the format changes).
- Single player turn order is sequential; EndTurn is refused while units need orders (awake, moves left, no move order).
- Map generation is Sovereign's own (fractal noise + latitude climate); its knobs are `MAPGEN_*` globals in `setup.json`. `CITY_SIGHT_RANGE` = 2 is a Sovereign choice (Civ value unverified).
- Units can only path through plots their owner has revealed (Civ lets units path into the unknown; revisit if it feels bad in play).

## Pointers
- [core/CMakeLists.txt] — library `sovereign_core`, `sovsim` tool, `sovereign_tests`, CTest checks
- [core/include/sovereign/game.h] — `Game`: create/replay/fromScenario, submit/validate, queries (paths, move cost, blockers)
- [core/src/game.cpp] — command application, movement, multi-turn moves, visibility, turn cycle
- [core/src/mapgen.cpp] — map generator and start positions
- [core/src/serialize.cpp] — save format and loader validation
- [core/tools/random_bot.h] — dumb bot used by soak tests and `sovsim`
- [specs/civ6/10-ai-ui-implementation.md : L96-L116] — build order and testing strategy
- [specs/civ6/00-overview.md : L140-L180] — modifier system sketch; turn processing order
- [specs/civ6/02-cities.md] — MVP-2 rules (growth, tiles, borders, housing, amenities)

## Micro-steps
<!-- [ ] pending · [>] active · [x] done · [-] dropped (reason) · [!] blocked -->
1. [x] **Build** core foundations + MVP-1 (map, terrain/features/resources, fog of war, settler founds city, movement with A*, end-turn loop, commands, replay, saves, CI on Linux GCC/Clang and Windows MSVC) — done 2026-10-04: 30 tests pass on GCC and Clang, same state hash on both; PR "Rules core milestone 1"
2. [>] **Build** MVP-2 cities: the modifier system first, then yields per plot, citizens working tiles, growth and housing, border growth by culture, production queue (units, Monument, Granary), gold and maintenance, simple amenities — spec below
3. [ ] **Build** MVP-3 research: Ancient and Classical techs and civics, boosts, Chiefdom/Autocracy/Oligarchy/Classical Republic, policy cards
4. [ ] **Build** MVP-4 combat: combat formula and damage table, ZOC, ranged, city walls and city combat, capture/raze, barbarians, XP and promotions, war/peace state
5. [ ] **Build** MVP-5 districts: Campus, Holy Site, Commercial Hub, Encampment, Theater Square, Industrial Zone, adjacency, district limit, their buildings
6. [ ] **Build** MVP-6 AI: basic economic and military AI as command-issuing players, war/peace diplomacy; all-AI soak test to a winner
7. [ ] **Build** MVP-7 victory: Domination and Score, then Science (space projects); turn limit
8. [ ] **Close** slice: review against Acceptance, archive this plan, start build-plan step 2 (Unreal bridge, on James's PC)

### Step 2 spec — MVP-2 cities
- Goal: cities grow, work tiles, produce units and buildings, and pay for them, all through commands and data.
- Read first: `specs/civ6/02-cities.md`; `specs/civ6/00-overview.md` "Architecture recommendations for a clone" and "Turn processing order"; grep `specs/civ6/data/global-parameters.md` `## CITY`, `## CITIZEN`, `## CULTURE`, `## GOLD`; grep `buildings.md` for Monument, Granary; `eras-moments-loyalty.md` Amenities.
- Change: (a) `modifiers` module: modifier = collection + effect + owner/subject requirement sets + arguments, loaded from `data/rules/modifiers.json`; start with yield and housing effects. (b) City state: population, food and production buckets, worked plots, production queue, buildings, culture bucket for border growth. (c) Commands: SetProduction, BuyPlot (gold), SetWorkedPlot (manual citizen), PurchaseItem. (d) Start-of-turn city processing in the 00-overview order: yields, gold/maintenance (disband on debt), growth/starvation, production completion, border growth. (e) Generate buildings rows (Monument, Granary, Palace) with `gen_rules.py`; the Palace is granted with the capital. (f) Scenario tests for growth thresholds, border growth costs, production overflow, maintenance; update golden hash and bump `kSaveVersion`.
- Verify: `cmake --build core/build && ctest --test-dir core/build --output-on-failure`; `core/build/sovsim --rules data/rules --turns 100` shows cities growing.
- Done when: data tests assert Civ VI growth and border numbers from the tables; replay and save round-trip still exact; CI green on all three compilers.
- Risk: med — new state and many formulas; deterministic math must stay in `Fixed`.
- Out of scope: districts, research, combat, amenities from luxuries beyond the simple version.

## Acceptance
- `core/` builds with no Unreal dependency on GCC, Clang and MSVC with warnings as errors; CTest green.
- Every rule value comes from `data/rules/`; changing a data row changes behaviour without recompiling.
- Replay of the command log and save/load reproduce the exact state hash; golden hash identical across compilers.
- An all-AI headless game runs to a victory (after MVP-6/7).

## Stage 4 review log
- Step 1 (2026-10-04): self-reviewed; GCC and Clang builds warning-free, 30/30 tests, `sovsim` 60-turn replay hashes equal on both compilers. MSVC verified by CI.

## Open questions & risks
- MSVC warning level /W4 /WX may flag narrowing conversions the GCC build accepts; fix as CI reports.
- River generation is an approximation (left-bank edges of a downhill tile path); good enough for movement and fresh water, revisit with the world generator.
- Lakes are not generated yet (inland water is coast); floodplains need rivers, which exist.

## Changelog
- 2026-10-04 18:05 CREATED — 8 steps from the build plan; step 1 implemented in the same session.
- 2026-10-04 18:05 STEP 1 DONE — milestone 1 PR opened; step 2 (MVP-2 cities) specced.
