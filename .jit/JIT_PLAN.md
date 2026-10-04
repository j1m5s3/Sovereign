# Headless rules core, build-plan step 1 (Civ core through MVP-7)

status: IN_PROGRESS · slice: rules-core · base: ba79f37 · created: 2026-10-04 · updated: 2026-10-04 19:10

Build the engine-independent C++ rules core (`core/`) through MVP-7 of `specs/civ6/10-ai-ui-implementation.md` ("Recommended build order"), with tests, as build-plan step 1 of `specs/sovereign/engine-and-architecture.md`. Each milestone is one PR, merged to `main` once CI is green. Out of scope for this slice: the Unreal bridge (step 2, needs James's PC), the leader character (step 3), live scenes, art.

## Decisions (do not reopen)
- Core is plain C++17, no Unreal headers, builds standalone with CMake; namespace `sov`. Enforced by `tools/check_core_rules.py` (no float/double, `<random>`, unordered containers, wall clock, `<cmath>`, Unreal includes), which runs in CTest.
- Numbers: `sov::Fixed`, int64 with 4 decimal places; data numbers are parsed from text, never through a float. 128-bit intermediates via `__int128` (GCC/Clang) or `_mul128/_div128` (MSVC x64).
- RNG: xoshiro256** per stream (MapGen, Gameplay, Combat, AI, Visual), seeded from the game seed via splitmix64; own `below()` with rejection sampling.
- Hex grid: pointy-top, odd-row offset storage (odd rows shifted east), axial math, east-west wrap; direction order NE, E, SE, SW, W, NW. River flags live on each plot's E/SE/SW edges.
- Every state change is a `sov::Command` (flat POD) through `Game::submit`; rejected commands are not logged. `Game::replay(setup, log)` must reproduce the state hash exactly.
- Rules data: JSON in `data/rules/`. `globals.json`, `terrain.json`, `resources.json`, `units.json` are generated from `specs/civ6/data` by `tools/rules_gen/gen_rules.py` (CTest checks they are current); `civilizations.json` (Sovereign's 12 civs) and `setup.json` are hand-written. Extra rules directories layer on top by row id (mods); `"delete": true` removes a row. The rules checksum is stored in saves.
- Saves: magic `SOVS`, `kSaveVersion` (2 since MVP-2), rules checksum, state, command log; little-endian explicit widths. Golden hash `core/tests/golden/duel_seed2026_30turns.hash` must match on GCC, Clang and MSVC; regenerate with `SOVEREIGN_UPDATE_GOLDEN=1` only for intended rules/format changes (bump `kSaveVersion` when the format changes).
- Single player turn order is sequential; EndTurn is refused while units need orders (awake, moves left, no move order).
- Map generation is Sovereign's own (fractal noise + latitude climate); its knobs are `MAPGEN_*` globals in `setup.json`. `CITY_SIGHT_RANGE` = 2 is a Sovereign choice (Civ value unverified).
- `Command` fields: type, player, id (unit or city), target, arg, arg2. Production items are (kind, type index); indices are safe because the rules checksum must match.
- Modifiers are hand-written in `data/rules/modifiers.json` with a `source` id (building, civ, or EVERYONE); the generated building table carries only flat values.
- `Game::fromScenario` does not run start-of-turn city processing (scenario states count as already processed).
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
2. [x] **Build** MVP-2 cities — done 2026-10-04: modifier system (`core/src/modifiers.cpp`; sources: building, civ, EVERYONE; collections OWNER_CITY(_PLOTS), PLAYER_CITIES, PLAYER_CAPITAL, PLAYER_CITY_PLOTS; effects city/plot yield, yield %, housing, amenities, growth %, defense), buildings generated from `buildings.md`, `Fixed::pow` (log2/exp2), city yields/citizens/growth/housing/amenities/mood, border growth, production queue with per-item progress and overflow, gold purchase, plot purchase, locked citizens, gold upkeep and bankruptcy disbanding; save v2; 44 tests on GCC and Clang
3. [>] **Build** MVP-3 research: Ancient and Classical techs and civics, boosts, Chiefdom/Autocracy/Oligarchy/Classical Republic, policy cards — spec below
4. [ ] **Build** Builders, improvements (farm, mine, quarry, pasture, plantation, camp, fishing boats) and roads; luxury amenities; strategic resources revealed by tech
5. [ ] **Build** MVP-4 combat: combat formula and damage table, ZOC, ranged, city walls and city combat, capture/raze, barbarians, XP and promotions, war/peace state
6. [ ] **Build** MVP-5 districts: Campus, Holy Site, Commercial Hub, Encampment, Theater Square, Industrial Zone, adjacency, district limit, their buildings
7. [ ] **Build** MVP-6 AI: basic economic and military AI as command-issuing players, war/peace diplomacy; all-AI soak test to a winner
8. [ ] **Build** MVP-7 victory: Domination and Score, then Science (space projects); turn limit
9. [ ] **Close** slice: review against Acceptance, archive this plan, start build-plan step 2 (Unreal bridge, on James's PC)

### Step 3 spec — MVP-3 research
- Goal: players research techs and civics, earn boosts, adopt a government and slot policy cards; unlocks gate units, buildings and resources.
- Read first: `specs/civ6/04-tech-civics-government.md`; grep `technologies.md`, `civics.md`, `governments-policies.md` for Ancient and Classical rows; `global-parameters.md` `## TECH`, `## CIVIC`, `## POLICY`, `## GOVERNMENT`.
- Change: (a) generate `techs.json`, `civics.json`, `governments.json`, `policies.json` (all eras, so later milestones only add behaviour); map the display-name `unlock` strings on units/buildings/resources to tech/civic ids in the generator. (b) Player state: researched sets, current research/civic with progress per item, boosts earned, government, slotted policies. (c) Commands: ChooseResearch, ChooseCivic, ChangeGovernment, SetPolicies. (d) Turn processing step 4: science/culture into current items with overflow; end-turn blockers "Choose research/civic". (e) Boost triggers for the Ancient/Classical boosts that MVP-2 state can detect (found city, train unit X, build building Y, own N cities); others stay inert with a data flag. (f) Policies and governments apply through the modifier system (add effects as needed: unit production %, building production %, combat strength later). (g) Remove the Player science/culture lifetime totals added in MVP-2 (replaced by research). (h) Bump `kSaveVersion`, update golden.
- Verify: `ctest --test-dir core/build --output-on-failure`; `sovsim --turns 150 --cities` shows techs researched and governments adopted.
- Done when: data tests assert tech costs and the 40% (Ancient) boost; scenario tests for research overflow, boost, policy effect; CI green on all three compilers.
- Risk: med — wide data mapping; keep effects data-driven.
- Out of scope: Great People, religion, later-era policies' special effects.

## Acceptance
- `core/` builds with no Unreal dependency on GCC, Clang and MSVC with warnings as errors; CTest green.
- Every rule value comes from `data/rules/`; changing a data row changes behaviour without recompiling.
- Replay of the command log and save/load reproduce the exact state hash; golden hash identical across compilers.
- An all-AI headless game runs to a victory (after MVP-6/7).

## Stage 4 review log
- Step 1 (2026-10-04): self-reviewed; GCC and Clang builds warning-free, 30/30 tests, `sovsim` 60-turn replay hashes equal on both compilers. MSVC CI caught CRLF changing the rules checksum; fixed (checksum skips CR, `.gitattributes`). PR #8 merged.
- Step 2 (2026-10-04): self-reviewed; 44/44 tests on GCC and Clang, 150-turn 4-player `sovsim` hash equal on both.

## Open questions & risks
- MSVC warning level /W4 /WX may flag narrowing conversions the GCC build accepts; fix as CI reports.
- River generation is an approximation (left-bank edges of a downhill tile path); good enough for movement and fresh water, revisit with the world generator.
- Lakes are not generated yet (inland water is coast); floodplains need rivers, which exist.
- `Fixed::pow` snaps results within ~2e-9 relative of an integer so floors stay exact; non-integer results carry ~1e-4 absolute error at most for rules-sized inputs.
- Unassigned citizens (more pop than workable plots) yield nothing until specialists arrive with districts.
- Gold purchase price uses GOLD_PURCHASE_MULTIPLIER (2) x GOLD_PURCHASE_ENGINE_FACTOR (2, Sovereign global) to match the spec's 4x; verify in game if possible.

## Changelog
- 2026-10-04 18:05 CREATED — 8 steps from the build plan; step 1 implemented in the same session.
- 2026-10-04 18:05 STEP 1 DONE — milestone 1 PR opened; step 2 (MVP-2 cities) specced.
- 2026-10-04 19:10 STEP 2 DONE — added step 4 (builders, improvements, roads) since MVP-2 cities need improvements before luxuries and strategics matter; step 3 (research) specced.
