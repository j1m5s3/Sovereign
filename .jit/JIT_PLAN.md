# Headless rules core, build-plan step 1 (Civ core through MVP-7)

status: IN_PROGRESS · slice: rules-core · base: ba79f37 · created: 2026-10-04 · updated: 2026-10-04 23:00

Build the engine-independent C++ rules core (`core/`) through MVP-7 of `specs/civ6/10-ai-ui-implementation.md` ("Recommended build order"), with tests, as build-plan step 1 of `specs/sovereign/engine-and-architecture.md`. Each milestone is one PR, merged to `main` once CI is green. Out of scope for this slice: the Unreal bridge (step 2, needs James's PC), the leader character (step 3), live scenes, art.

## Decisions (do not reopen)
- Core is plain C++17, no Unreal headers, builds standalone with CMake; namespace `sov`. Enforced by `tools/check_core_rules.py` (no float/double, `<random>`, unordered containers, wall clock, `<cmath>`, Unreal includes), which runs in CTest.
- Numbers: `sov::Fixed`, int64 with 4 decimal places; data numbers are parsed from text, never through a float. 128-bit intermediates via `__int128` (GCC/Clang) or `_mul128/_div128` (MSVC x64).
- RNG: xoshiro256** per stream (MapGen, Gameplay, Combat, AI, Visual), seeded from the game seed via splitmix64; own `below()` with rejection sampling.
- Hex grid: pointy-top, odd-row offset storage (odd rows shifted east), axial math, east-west wrap; direction order NE, E, SE, SW, W, NW. River flags live on each plot's E/SE/SW edges.
- Every state change is a `sov::Command` (flat POD) through `Game::submit`; rejected commands are not logged. `Game::replay(setup, log)` must reproduce the state hash exactly.
- Rules data: JSON in `data/rules/`. `globals`, `terrain`, `resources`, `promotions`, `units`, `buildings`, `techs`, `civics`, `governments`, `policies` and `improvements.json` are generated from `specs/civ6/data` by `tools/rules_gen/gen_rules.py` (CTest checks they are current); `civilizations.json` (Sovereign's 12 civs), `setup.json` and `modifiers.json` are hand-written. Extra rules directories layer on top by row id (mods); `"delete": true` removes a row. The rules checksum is stored in saves.
- Saves: magic `SOVS`, `kSaveVersion` (5 since step 5a), rules checksum, state, command log; little-endian explicit widths. Golden hash `core/tests/golden/duel_seed2026_30turns.hash` must match on GCC, Clang and MSVC; regenerate with `SOVEREIGN_UPDATE_GOLDEN=1` only for intended rules/format changes (bump `kSaveVersion` when the format changes).
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
- Combat (step 5a): promotions and unit abilities are generated into `promotions.json`; their effect text is parsed into typed effects whose conditions are "all of (any of ...)" groups; unparsed effects and conditions are `UNTRACKED` and never apply. Units list their innate abilities; modifiers grant more (`GRANT_ABILITY`, collection PLAYER, filtered by the ability's classes).
- Damage = round((COMBAT_BASE_DAMAGE + roll 0..COMBAT_MAX_EXTRA_DAMAGE) × e^(COMBAT_POWER_SCALING × diff)), at least COMBAT_MINIMUM_DAMAGE, via `Fixed::exp`; rolls come from the Combat stream (defender's damage first). COMBAT_POWER_DAMPENER and COMBAT_DAMAGE_MULTIPLIER_MINIMUM are not applied (meaning unverified).
- The river penalty applies to melee attacks only; flanking to melee attacks only; support to every defence. Wounded penalty = round(COMBAT_WOUNDED_DAMAGE_MULTIPLIER × missing HP / max HP), halves up.
- XP per combat = floor(enemy base × (KILL_BONUS if it died) / own base) + 2 melee or 1 ranged + 1 attacker, capped at 8, then × (100 + XP bonus %) / 100. XP stops at the next level until a promotion is taken; promoting zeroes XP, heals 50 and ends the turn. Next level = `EXPERIENCE_PER_LEVEL` (Sovereign global, 15) × level; EXPERIENCE_MAX_LEVEL is not enforced.
- War and peace: DeclareWar needs DIPLOMACY_PEACE_MIN_TURNS since the last peace; peace needs DIPLOMACY_WAR_MIN_TURNS of war and both sides' MakePeace. Closed borders (civic flag ENFORCE_BORDERS, Early Empire) stop other players' units entering unless at war or the unit ignores borders.
- Melee into a plot with only enemy civilians captures them (`capturedAs`) or destroys them; a melee kill advances the attacker and captures escorted civilians. Ranged units cannot attack civilians. Healing runs at the start of the owner's turn for units that neither moved nor attacked the turn before.
- Strategic unit maintenance is paid at turn start; a shortfall empties the stockpile and gives that resource's units −20 strength and no healing until paid.

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
5. [x] **Build** MVP-4a unit combat — done 2026-10-04: generated `promotions.json` (promotions and abilities with typed effects), unit bombard, fuel maintenance, capture results, civic flags; `src/combat.cpp` (war and peace, strength with terrain, river, fortify, flanking/support, matchups, wounds and fuel, damage via `Fixed::exp`, melee/ranged attacks, capture, ZOC, closed borders, XP, promotions, healing); Oligarchy and Survey modifiers; save v5; bots declare war, fight and promote; 70 tests on GCC and Clang
6. [>] **Build** MVP-4b city combat and barbarians — spec below
7. [ ] **Build** MVP-5 districts: Campus, Holy Site, Commercial Hub, Encampment, Theater Square, Industrial Zone, adjacency, district limit, their buildings
8. [ ] **Build** MVP-6 AI: basic economic and military AI as command-issuing players, war/peace diplomacy; all-AI soak test to a winner
9. [ ] **Build** MVP-7 victory: Domination and Score, then Science (space projects); turn limit
10. [ ] **Close** slice: review against Acceptance, archive this plan, start build-plan step 2 (Unreal bridge, on James's PC)

### Step 6 spec — MVP-4b city combat and barbarians
- Goal: cities fight and fall; barbarians raid.
- Read first: `specs/civ6/05-units-and-combat.md` (Damage formula: Walls; XP), `specs/civ6/02-cities.md` (city combat, capture), grep `global-parameters.md` `## COMBAT` (CITY_*, HEAL_CITY_*), `## BARBARIAN`; `barbarians-goody-huts.md`; `buildings.md` rows for walls (`outerDefenseHp`, `defense`); `core/src/combat.cpp`.
- Change: (a) City combat strength (strongest unit or garrison basis per spec, + walls `defense`, + `ADJUST_CITY_DEFENSE` modifiers), city HP (center 200) and outer defense HP from walls; walls absorb damage first at the melee/ranged/bombard percentages; Battering Ram and Siege Tower abilities. (b) Attack and RangedAttack on city plots (ranged −17 via `RangedVsDistrict`); city ranged strike once walls exist (new command, range 2, COMBAT_CITY_RANGED_DAMAGE_THRESHOLD), city and wall healing with the cooldown. (c) Capture by melee into a 0-HP city: ownership, 25% population loss, buildings per spec, capital rules, raze choice (command); XP for capture/district attacks; a player with no cities and no units is eliminated. (d) Barbarians as a player always at war: camps placed by map gen (spacing globals), spawn scouts and raiders, simple attack behaviour, camp clearing; Discipline (+5 vs barbarians) and the barbarian XP level cap. (e) `VsDistrict` and "plot district is defended" conditions. (f) Save version bump and golden.
- Verify: `ctest --test-dir core/build --output-on-failure`; `sovsim --turns 200 --cities` shows captured cities and barbarian kills; GCC and Clang hashes equal.
- Done when: tests cover wall damage split, city capture with population loss, city strike, raze, barbarian camp spawn and clearing; CI green on all three compilers.
- Risk: high — many interacting rules; keep barbarian AI minimal (MVP-6 owns real AI) → coder: opus
- Out of scope: Encampment district combat (step 7), loyalty flips, Great Generals, corps/armies, air, naval movement.

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
- Step 5a (2026-10-04): self-reviewed; 70/70 tests on GCC and Clang; 250-turn 6-player `sovsim` hash equal on both (3e2a5ba957975109) with 26 wars, 135 attacks and 7 promotions.

## Open questions & risks
- MSVC warning level /W4 /WX may flag narrowing conversions the GCC build accepts; fix as CI reports.
- River generation is an approximation (left-bank edges of a downhill tile path); good enough for movement and fresh water, revisit with the world generator.
- Lakes are not generated yet (inland water is coast); floodplains need rivers, which exist.
- `Fixed::pow` snaps results within ~2e-9 relative of an integer so floors stay exact; non-integer results carry ~1e-4 absolute error at most for rules-sized inputs.
- Unassigned citizens (more pop than workable plots) yield nothing until specialists arrive with districts.
- Paid government and policy changes (POLICY_COST_*) are not implemented: the formula is unverified. Anarchy length is a Sovereign reading of GOVERNMENT_BASE_ANARCHY_TURNS.
- Influence, loyalty, great person, wonder and specialty-district policy effects (Charismatic Leader, Classical Republic...) have no modifiers yet; they arrive with those systems. Discipline waits for barbarians (step 6).
- 95 of 205 promotion/ability effects and 42 conditions are UNTRACKED (movement-cost, healing, plunder, adjacency auras...); fill them in as their systems arrive.
- No naval movement or embarkation yet, so naval combat, amphibious penalties and naval healing are untested.
- Strategic unit maintenance per turn, stockpile cap bonuses from Barracks/Stable/Armory/Military Academy, and harvest scaling with tree progress are not modelled yet.
- The GS world-era tech/civic cost adjustment (±20%) waits for world eras.
- Gold purchase price uses GOLD_PURCHASE_MULTIPLIER (2) x GOLD_PURCHASE_ENGINE_FACTOR (2, Sovereign global) to match the spec's 4x; verify in game if possible.

## Changelog
- 2026-10-04 18:05 CREATED — 8 steps from the build plan; step 1 implemented in the same session.
- 2026-10-04 18:05 STEP 1 DONE — milestone 1 PR opened; step 2 (MVP-2 cities) specced.
- 2026-10-04 19:10 STEP 2 DONE — added step 4 (builders, improvements, roads) since MVP-2 cities need improvements before luxuries and strategics matter; step 3 (research) specced.
- 2026-10-04 20:30 STEP 3 DONE — step 4 (builders, improvements, roads) specced; it also takes strategic resource costs and obsolete units, found missing while wiring unlocks.
- 2026-10-04 21:30 STEP 4 DONE — roads dropped from the step (traders build them with trade routes); step 5 (MVP-4 combat) specced, rated high risk and may split into two PRs.
- 2026-10-04 23:00 STEP 5a DONE — MVP-4 split as planned: unit combat, war/peace, ZOC, XP and promotions shipped; step 6 (MVP-4b city combat and barbarians) specced; later steps renumbered.
