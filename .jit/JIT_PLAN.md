# Headless rules core, build-plan step 1 (Civ core through MVP-7)

status: IN_PROGRESS · slice: rules-core · base: ba79f37 · created: 2026-10-04 · updated: 2026-10-04 21:30

Build the engine-independent C++ rules core (`core/`) through MVP-7 of `specs/civ6/10-ai-ui-implementation.md` ("Recommended build order"), with tests, as build-plan step 1 of `specs/sovereign/engine-and-architecture.md`. Each milestone is one PR, merged to `main` once CI is green. Out of scope for this slice: the Unreal bridge (step 2, needs James's PC), the leader character (step 3), live scenes, art.

## Decisions (do not reopen)
- Core is plain C++17, no Unreal headers, builds standalone with CMake; namespace `sov`. Enforced by `tools/check_core_rules.py` (no float/double, `<random>`, unordered containers, wall clock, `<cmath>`, Unreal includes), which runs in CTest.
- Numbers: `sov::Fixed`, int64 with 4 decimal places; data numbers are parsed from text, never through a float. 128-bit intermediates via `__int128` (GCC/Clang) or `_mul128/_div128` (MSVC x64).
- RNG: xoshiro256** per stream (MapGen, Gameplay, Combat, AI, Visual), seeded from the game seed via splitmix64; own `below()` with rejection sampling.
- Hex grid: pointy-top, odd-row offset storage (odd rows shifted east), axial math, east-west wrap; direction order NE, E, SE, SW, W, NW. River flags live on each plot's E/SE/SW edges.
- Every state change is a `sov::Command` (flat POD) through `Game::submit`; rejected commands are not logged. `Game::replay(setup, log)` must reproduce the state hash exactly.
- Rules data: JSON in `data/rules/`. `globals`, `terrain`, `resources`, `units`, `buildings`, `techs`, `civics`, `governments` and `policies.json` are generated from `specs/civ6/data` by `tools/rules_gen/gen_rules.py` (CTest checks they are current); `civilizations.json` (Sovereign's 12 civs), `setup.json` and `modifiers.json` are hand-written. Extra rules directories layer on top by row id (mods); `"delete": true` removes a row. The rules checksum is stored in saves.
- Saves: magic `SOVS`, `kSaveVersion` (2 since MVP-2), rules checksum, state, command log; little-endian explicit widths. Golden hash `core/tests/golden/duel_seed2026_30turns.hash` must match on GCC, Clang and MSVC; regenerate with `SOVEREIGN_UPDATE_GOLDEN=1` only for intended rules/format changes (bump `kSaveVersion` when the format changes).
- Single player turn order is sequential; EndTurn is refused while units need orders (awake, moves left, no move order).
- Map generation is Sovereign's own (fractal noise + latitude climate); its knobs are `MAPGEN_*` globals in `setup.json`. `CITY_SIGHT_RANGE` = 2 is a Sovereign choice (Civ value unverified).
- `Command` fields: type, player, id (unit or city), target, arg, arg2. Production items are (kind, type index); indices are safe because the rules checksum must match.
- Modifiers are hand-written in `data/rules/modifiers.json` with a `source` id (building, civ, or EVERYONE); the generated building table carries only flat values.
- `Game::fromScenario` does not run start-of-turn city processing (scenario states count as already processed).
- Research (MVP-3): tech/civic/government/policy data is generated (`techs.json` also holds `eras`); unit and building unlocks are tech/civic ids. Progress is per node; overflow carries into the next choice; one completion per tree per turn. EndTurn is refused (ResearchNeeded/CivicNeeded) only once the player owns a city.
- Boost conditions are generated into typed triggers and checked against state after every command and at turn start; `UNTRACKED` and district/improvement/combat triggers stay inert until those systems exist. Future-era nodes without prerequisites need the whole Information era (Civ randomises them).
- Government and policy changes are free only on a turn a civic completes, on first adoption, and when anarchy ends; paid changes wait until the gold formula is verified. Changing government empties all slots. Returning to a used government costs `GOVERNMENT_BASE_ANARCHY_TURNS` + times used turns of anarchy (no gold, science, culture, faith, government or policy effects).
- A policy card retires once any card in its `obsoletedBy` list (replacement cards) is unlocked.
- Improvements (step 4): generated `improvements.json` holds the Builder-built, non-unique rows. A plot with a visible resource takes only improvements listed for that resource; otherwise a feature must be in `validFeatures`, else the terrain in `validTerrains`. Improving and harvesting need the plot in the player's territory, use a charge and end the builder's moves. Harvest yields are base × game speed (Civ's scaling with tree progress is unverified) and need an unimproved plot.
- Luxuries: each improved luxury type gives +1 amenity to `amenityCities` cities, largest first (ties: oldest) — a Sovereign stand-in for Civ's "neediest first". City centers count as improving their resource.
- Strategics follow the GS stockpile model: improved sources add `accumulation` per turn up to `stockpileCap`; training or buying a unit needs and spends `strategicCost`, and production waits while short. A unit cannot be trained once its upgrade can be (unlocked and resource on hand) or its `obsoleteWith` is known.
- Roads are deferred to trade routes (traders build them); Builders do not build roads in Civ VI.
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
3. [x] **Build** MVP-3 research — done 2026-10-04: generated techs (77), civics (61), eras, governments (13), policies (140); `src/research.cpp` (choices, overflow, boosts, governments, anarchy, free change windows, policy slots and obsolescence); unlock gating for units, buildings and resource yields; new modifier sources (policy, government), collection PLAYER and effects unit production %, plot purchase cost %, unit maintenance discount; Ancient/Classical policy and Autocracy modifiers; save v3; 51 tests on GCC and Clang
4. [x] **Build** Builders and improvements — done 2026-10-04: generated `improvements.json` (17 rows), feature/resource harvest data, strategic costs, upgrades and forced obsolescence; `src/improvements.cpp` (placement, BuildImprovement and Harvest commands, improvement yields with tech bonuses and farm adjacency, improvement housing, luxury amenities, strategic stockpiles); improvement boosts now fire; save v4; 58 tests on GCC and Clang
5. [>] **Build** MVP-4 combat: combat formula and damage table, ZOC, ranged, city walls and city combat, capture/raze, barbarians, XP and promotions, war/peace state
6. [ ] **Build** MVP-5 districts: Campus, Holy Site, Commercial Hub, Encampment, Theater Square, Industrial Zone, adjacency, district limit, their buildings
7. [ ] **Build** MVP-6 AI: basic economic and military AI as command-issuing players, war/peace diplomacy; all-AI soak test to a winner
8. [ ] **Build** MVP-7 victory: Domination and Score, then Science (space projects); turn limit
9. [ ] **Close** slice: review against Acceptance, archive this plan, start build-plan step 2 (Unreal bridge, on James's PC)

### Step 5 spec — MVP-4 combat
- Goal: units fight. Melee, ranged and city combat with Civ's damage formula; zone of control; capture and raze; war and peace between players; barbarians; XP and promotions.
- Read first: `specs/civ6/05-units-and-combat.md` (Combat resolution, Strength calculation, Damage formula, XP and promotions, Healing, Barbarian AI combat), `specs/civ6/02-cities.md` (City combat), grep `global-parameters.md` `## COMBAT`, `## EXPERIENCE`, `## BARBARIAN`; `promotions.md` and `barbarians-goody-huts.md` for rows.
- Change: (a) diplomacy state: war/peace per player pair, DeclareWar and MakePeace commands (peace only after a minimum war length from data, if any). (b) Combat: strength with terrain, river, flanking/support (after Military Tradition), fortify, damage-reduced strength; damage = 30·e^(diff/25)·random(0.8–1.2) done in fixed point with the Combat RNG stream; Attack command for melee (move in when the defender dies) and RangedAttack. (c) ZOC stops movement next to enemy combat units. (d) Cities: HP, walls (outer defense pool), city ranged strike once walls exist, capture by melee into a 0-HP city and raze (choose on capture), healing. (e) XP gain and promotion choice per class from `promotions.md`; healing rules. (f) Barbarians: outposts placed by map gen, spawn scouts and units, attack. (g) Combat modifiers through the modifier system (Discipline, Oligarchy, Survey XP...). (h) Unit strategic maintenance shortfall −20 strength (data). (i) Bump `kSaveVersion`, golden.
- Verify: `ctest --test-dir core/build --output-on-failure`; a scripted duel scenario and `sovsim --turns 200` with bots declaring war produce kills and captures; replay matches.
- Done when: tests reproduce damage values for known strength differences, ZOC, city capture with walls, promotion; CI green on all three compilers.
- Risk: high — the core loop's biggest rule set; split into two PRs (units combat + war/peace + ZOC + XP, then cities + barbarians) if it runs long.
- Out of scope: air, nuclear, religion combat, Great Generals, corps/armies, live battle scenes (build-plan step 5).

## Acceptance
- `core/` builds with no Unreal dependency on GCC, Clang and MSVC with warnings as errors; CTest green.
- Every rule value comes from `data/rules/`; changing a data row changes behaviour without recompiling.
- Replay of the command log and save/load reproduce the exact state hash; golden hash identical across compilers.
- An all-AI headless game runs to a victory (after MVP-6/7).

## Stage 4 review log
- Step 1 (2026-10-04): self-reviewed; GCC and Clang builds warning-free, 30/30 tests, `sovsim` 60-turn replay hashes equal on both compilers. MSVC CI caught CRLF changing the rules checksum; fixed (checksum skips CR, `.gitattributes`). PR #8 merged.
- Step 2 (2026-10-04): self-reviewed; 44/44 tests on GCC and Clang, 150-turn 4-player `sovsim` hash equal on both. CI caught a missing `<algorithm>` include (libc++ and MSVC); fixed.
- Step 3 (2026-10-04): self-reviewed; CI green on all three compilers; PR #10 merged; 51/51 tests on GCC and Clang, 150-turn 4-player `sovsim` hash equal on both (02d7599831f383ca); bots research, adopt governments and slot policies.

- Step 4 (2026-10-04): self-reviewed; 58/58 tests on GCC and Clang, 150-turn 4-player `sovsim` hash equal on both (60ad1eb6d65f6049); bots build improvements.

## Open questions & risks
- MSVC warning level /W4 /WX may flag narrowing conversions the GCC build accepts; fix as CI reports.
- River generation is an approximation (left-bank edges of a downhill tile path); good enough for movement and fresh water, revisit with the world generator.
- Lakes are not generated yet (inland water is coast); floodplains need rivers, which exist.
- `Fixed::pow` snaps results within ~2e-9 relative of an integer so floors stay exact; non-integer results carry ~1e-4 absolute error at most for rules-sized inputs.
- Unassigned citizens (more pop than workable plots) yield nothing until specialists arrive with districts.
- Paid government and policy changes (POLICY_COST_*) are not implemented: the formula is unverified. Anarchy length is a Sovereign reading of GOVERNMENT_BASE_ANARCHY_TURNS.
- Combat, influence, loyalty, great person, wonder and specialty-district policy effects (Discipline, Oligarchy, Charismatic Leader, Classical Republic...) have no modifiers yet; they arrive with those systems.
- Strategic unit maintenance per turn, stockpile cap bonuses from Barracks/Stable/Armory/Military Academy, and harvest scaling with tree progress are not modelled yet.
- The GS world-era tech/civic cost adjustment (±20%) waits for world eras.
- Gold purchase price uses GOLD_PURCHASE_MULTIPLIER (2) x GOLD_PURCHASE_ENGINE_FACTOR (2, Sovereign global) to match the spec's 4x; verify in game if possible.

## Changelog
- 2026-10-04 18:05 CREATED — 8 steps from the build plan; step 1 implemented in the same session.
- 2026-10-04 18:05 STEP 1 DONE — milestone 1 PR opened; step 2 (MVP-2 cities) specced.
- 2026-10-04 19:10 STEP 2 DONE — added step 4 (builders, improvements, roads) since MVP-2 cities need improvements before luxuries and strategics matter; step 3 (research) specced.
- 2026-10-04 20:30 STEP 3 DONE — step 4 (builders, improvements, roads) specced; it also takes strategic resource costs and obsolete units, found missing while wiring unlocks.
- 2026-10-04 21:30 STEP 4 DONE — roads dropped from the step (traders build them with trade routes); step 5 (MVP-4 combat) specced, rated high risk and may split into two PRs.
