# Headless rules core, build-plan step 1 (Civ core through MVP-7)

status: IN_PROGRESS · slice: rules-core · base: ba79f37 · created: 2026-10-04 · updated: 2026-10-05 01:30

Build the engine-independent C++ rules core (`core/`) through MVP-7 of `specs/civ6/10-ai-ui-implementation.md` ("Recommended build order"), with tests, as build-plan step 1 of `specs/sovereign/engine-and-architecture.md`. Each milestone is one PR, merged to `main` once CI is green. Out of scope for this slice: the Unreal bridge (step 2, needs James's PC), the leader character (step 3), live scenes, art.

## Decisions (do not reopen)
- Core is plain C++17, no Unreal headers, builds standalone with CMake; namespace `sov`. Enforced by `tools/check_core_rules.py` (no float/double, `<random>`, unordered containers, wall clock, `<cmath>`, Unreal includes), which runs in CTest.
- Numbers: `sov::Fixed`, int64 with 4 decimal places; data numbers are parsed from text, never through a float. 128-bit intermediates via `__int128` (GCC/Clang) or `_mul128/_div128` (MSVC x64).
- RNG: xoshiro256** per stream (MapGen, Gameplay, Combat, AI, Visual), seeded from the game seed via splitmix64; own `below()` with rejection sampling.
- Hex grid: pointy-top, odd-row offset storage (odd rows shifted east), axial math, east-west wrap; direction order NE, E, SE, SW, W, NW. River flags live on each plot's E/SE/SW edges.
- Every state change is a `sov::Command` (flat POD) through `Game::submit`; rejected commands are not logged. `Game::replay(setup, log)` must reproduce the state hash exactly.
- Rules data: JSON in `data/rules/`. `globals`, `terrain`, `resources`, `promotions`, `units`, `buildings`, `districts`, `barbarians`, `techs`, `civics`, `governments`, `policies` and `improvements.json` are generated from `specs/civ6/data` by `tools/rules_gen/gen_rules.py` (CTest checks they are current); `civilizations.json` (Sovereign's 12 civs), `setup.json` and `modifiers.json` are hand-written. Extra rules directories layer on top by row id (mods); `"delete": true` removes a row. The rules checksum is stored in saves.
- Saves: magic `SOVS`, `kSaveVersion` (6 since step 6), rules checksum, state, command log; little-endian explicit widths. Golden hash `core/tests/golden/duel_seed2026_30turns.hash` must match on GCC, Clang and MSVC; regenerate with `SOVEREIGN_UPDATE_GOLDEN=1` only for intended rules/format changes (bump `kSaveVersion` when the format changes).
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
- City combat (step 6): city HP comes from DISTRICT_CITY_CENTER (200); wall HP is the sum of the city's buildings' `outerDefenseHp`, added at full when a wall completes. City strength = max(strongest military unit the owner has ever had − CITY_STRENGTH_BELOW_STRONGEST_UNIT (Sovereign global, 10), garrison base strength) + building `defense` (3 per wall) + ADJUST_CITY_DEFENSE modifiers (Palace) + center terrain/feature defence − round(10 × missing HP / max). Attacks on a city plot always fight the city, never the garrison.
- Walls take hits first: melee 15%, ranged 50%, bombard 100% of the rolled damage, no spill-over to the city. `meleeCannotDamageWalls` (Medieval/Renaissance) makes melee 0%; an adjacent friendly WALL_FULL_DAMAGE unit (Battering Ram) makes it 100%; an adjacent BYPASS_WALLS unit (Siege Tower) sends melee to the city unless `wallsCannotBeBypassed` (Renaissance). Ranged class units fight cities at −17.
- Capture: a melee, anti-cavalry or cavalry unit (not barbarian) whose attack leaves the city at 0 HP, or that attacks a city already at 0, takes it with no fight: garrison killed, civilians seized, −25% population (floor, min 1), HP to 50%, walls 0, queue cleared, plots transfer, capital loses the Palace and the loser's oldest remaining city becomes capital. RazeCity (only on the capture turn, never an original capital) removes the city at once (Civ razes over turns). Liberation waits for diplomacy.
- CityStrike needs a wall building, once per turn, within the City Center `attackRange` (2), visible and in sight; strength is max(COMBAT_MINIMUM_CITY_STRIKE_STRENGTH, city strength). COMBAT_CITY_RANGED_DAMAGE_THRESHOLD is not applied (meaning unverified).
- City HP heals COMBAT_HEAL_CITY_GARRISON per turn unless besieged (all six neighbours hold an enemy or lie in enemy ZOC). Walls repair COMBAT_HEAL_CITY_OUTER_DEFENSES per turn once more than COMBAT_HEAL_OUTER_DEFENSES_COOLDOWN turns pass without an attack — a stand-in for Civ's Repair Outer Defenses project until projects exist.
- Elimination: a player with no cities who has lost a city (citiesFounded > 0), or has no units left, is out (checked after captures and unit losses); its units are removed and turns skip it.
- Barbarians (step 6): an extra last player (`barbarian`, civ none) when `GameSetup.barbarians` (default on), at war with everyone (war and peace with it are refused), sees the whole map, and acts inside the world turn without commands. Camps: on the first world turn a third of BARBARIAN_CAMP_MAX_PER_MAJOR_CIV × majors, then BARBARIAN_CAMP_ODDS_OF_NEW_CAMP_SPAWNING % per turn; unowned land not visible to any major, the BARBARIAN_CAMP_MINIMUM_DISTANCE_* gaps; tribe = first land tribe whose resource is in range (cavalry near Horses, else melee); naval tribes wait for naval movement. A camp releases a unit at once and every tribe `spawnTurns`, up to BARBARIAN_MAX_UNITS_PER_CAMP (Sovereign, 4): the strongest unit of the tribe's class (or RANGED at `rangedPercent`) that half the majors can build. Boldness +2/turn, +15/kill, −10/unit lost. Units attack at even or better odds (cities once boldness reaches `attackBoldness`), raid targets within 8 once past `raidBoldness`, else return home; units of a cleared camp roam at full boldness. No scouts or attack forces yet.
- Barbarians never take cities and turn captured Settlers into Builders; a military unit entering a camp clears it for BARBARIAN_CAMP_CLEAR_GOLD (Sovereign global, 50). Barbarian units gain no XP; fights with barbarians give no XP to units at EXPERIENCE_MAX_BARB_LEVEL (2) or above. Discipline is +5 strength vs barbarians (ADJUST_UNIT_STRENGTH with `vsBarbarians`).

## Pointers
- [core/CMakeLists.txt] — library `sovereign_core`, `sovsim` tool, `sovereign_tests`, CTest checks
- [core/include/sovereign/game.h] — `Game`: create/replay/fromScenario, submit/validate, queries (paths, move cost, blockers)
- [core/src/game.cpp] — command application, movement, multi-turn moves, visibility, turn cycle
- [core/src/mapgen.cpp] — map generator and start positions
- [core/src/serialize.cpp] — save format and loader validation
- [core/tools/random_bot.h] — dumb bot used by soak tests and `sovsim`
- [core/src/combat.cpp] — war and peace, unit and city combat, walls, capture, raze, elimination, healing
- [core/src/barbarians.cpp] — barbarian player, camps, unit release, raiders
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
6. [x] **Build** MVP-4b city combat and barbarians — done 2026-10-05: generated `districts.json` (City Center, Encampment HP and range) and `barbarians.json`, wall building flags; city HP, walls, strength, wall damage split with rams and siege towers, CityStrike and RazeCity commands, capture with population loss and Palace move, elimination, city and wall healing, siege; barbarian player, camps, unit release, raiders, camp clearing, Settler→Builder capture, Discipline, barbarian XP cap; save v6; bots strike, raze, hunt camps and take cities; 84 tests on GCC and Clang
7. [>] **Build** MVP-5 districts: Campus, Holy Site, Commercial Hub, Encampment, Theater Square, Industrial Zone, adjacency, district limit, their buildings — spec below
8. [ ] **Build** MVP-6 AI: basic economic and military AI as command-issuing players, war/peace diplomacy; all-AI soak test to a winner
9. [ ] **Build** MVP-7 victory: Domination and Score, then Science (space projects); turn limit
10. [ ] **Close** slice: review against Acceptance, archive this plan, start build-plan step 2 (Unreal bridge, on James's PC)

### Step 7 spec — MVP-5 districts
- Goal: cities place specialty districts and build in them.
- Read first: `.jit/JIT_INDEX.md` row "Districts, buildings, wonders"; `specs/civ6/03-districts-buildings-wonders.md` (Districts: general rules, District table, Example adjacency computation, Buildings); grep `specs/civ6/data/districts.md` `## District stats` and `## District adjacency, placement...` for the six districts plus their buildings in `buildings.md`; `core/src/city.cpp` (production), `tools/rules_gen/gen_rules.py` `gen_districts`.
- Change: (a) Generate the six base districts (cost, placement rules, adjacency rows, citizen slots, Encampment HP/strike) into `districts.json`; buildings already carry `district`. (b) City state: placed districts (type, plot, progress, pillaged later); a `PlaceDistrict` production item/command validated for plot (owned, within 3, not the center, terrain rules), one of each per city, the population limit (1 + floor((pop − 1) / DISTRICT_POPULATION_REQUIRED_PER)), cost scaling with tech/civic progress per spec. (c) Adjacency yields per the spec example (major/minor, rounding), district buildings need their district, specialist slots can wait. (d) Encampment: HP, its own strike and ZOC via the step 6 city-combat code. (e) `cityStrength` +2 per completed specialty district (Districts.CityStrengthModifier). (f) Save v7, golden, bot places districts.
- Verify: `ctest --test-dir core/build --output-on-failure` (GCC and Clang); `sovsim --seed 3 --players 6 --size MAPSIZE_SMALL --turns 250 --cities` shows districts and equal hashes on both compilers.
- Done when: tests cover placement rules, population limit, adjacency for Campus/Holy Site/Commercial Hub, a district building gated on its district, Encampment strike; CI green on all three compilers.
- Risk: med — new data path and placement rules; adjacency math has spec examples to test against → coder: sonnet
- Out of scope: wonders, unique districts, Neighborhood/Aqueduct/Harbor/Entertainment districts (later), great person points, pillaging, district projects.

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
- Step 6 (2026-10-05): self-reviewed; 84/84 tests on GCC and Clang; 250-turn 6-player `sovsim` hash equal on both (7f9afc343bada0b9) with 10 camps cleared and city strikes; 4-player tiny maps show captured cities and an eliminated player.

## Open questions & risks
- MSVC warning level /W4 /WX may flag narrowing conversions the GCC build accepts; fix as CI reports.
- River generation is an approximation (left-bank edges of a downhill tile path); good enough for movement and fresh water, revisit with the world generator.
- Lakes are not generated yet (inland water is coast); floodplains need rivers, which exist.
- `Fixed::pow` snaps results within ~2e-9 relative of an integer so floors stay exact; non-integer results carry ~1e-4 absolute error at most for rules-sized inputs.
- Unassigned citizens (more pop than workable plots) yield nothing until specialists arrive with districts.
- Paid government and policy changes (POLICY_COST_*) are not implemented: the formula is unverified. Anarchy length is a Sovereign reading of GOVERNMENT_BASE_ANARCHY_TURNS.
- Influence, loyalty, great person, wonder and specialty-district policy effects (Charismatic Leader, Classical Republic...) have no modifiers yet; they arrive with those systems. Discipline is in (step 6).
- 95 of 205 promotion/ability effects and 42 conditions are UNTRACKED (movement-cost, healing, plunder, adjacency auras...); fill them in as their systems arrive.
- No naval movement or embarkation yet, so naval combat, amphibious penalties and naval healing are untested.
- Strategic unit maintenance per turn, stockpile cap bonuses from Barracks/Stable/Armory/Military Academy, and harvest scaling with tree progress are not modelled yet.
- The GS world-era tech/civic cost adjustment (±20%) waits for world eras.
- Barbarian scouts, attack forces, coastal camps, Bronze Working camp boost and era score are not modelled; barbarian behaviour is a placeholder until MVP-6.
- Captured-city options are Keep or Raze (immediate); Liberate and occupation effects (no growth, loyalty 50) arrive with diplomacy and loyalty.
- Gold purchase price uses GOLD_PURCHASE_MULTIPLIER (2) x GOLD_PURCHASE_ENGINE_FACTOR (2, Sovereign global) to match the spec's 4x; verify in game if possible.

## Changelog
- 2026-10-04 18:05 CREATED — 8 steps from the build plan; step 1 implemented in the same session.
- 2026-10-04 18:05 STEP 1 DONE — milestone 1 PR opened; step 2 (MVP-2 cities) specced.
- 2026-10-04 19:10 STEP 2 DONE — added step 4 (builders, improvements, roads) since MVP-2 cities need improvements before luxuries and strategics matter; step 3 (research) specced.
- 2026-10-04 20:30 STEP 3 DONE — step 4 (builders, improvements, roads) specced; it also takes strategic resource costs and obsolete units, found missing while wiring unlocks.
- 2026-10-04 21:30 STEP 4 DONE — roads dropped from the step (traders build them with trade routes); step 5 (MVP-4 combat) specced, rated high risk and may split into two PRs.
- 2026-10-04 23:00 STEP 5a DONE — MVP-4 split as planned: unit combat, war/peace, ZOC, XP and promotions shipped; step 6 (MVP-4b city combat and barbarians) specced; later steps renumbered.
- 2026-10-05 01:30 STEP 6 DONE — city combat and barbarians shipped; Encampment combat moved into step 7 with the districts; step 7 (MVP-5 districts) specced.
