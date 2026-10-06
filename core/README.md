# Sovereign rules core

The engine-independent C++ library that owns every game rule (see
[engine-and-architecture.md](../specs/sovereign/engine-and-architecture.md)).
Unreal will compile these same sources as a module; here they build
standalone for headless tests, balance runs and AI self-play.

## Build and test

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build --parallel
ctest --test-dir core/build --output-on-failure
```

On Windows with Visual Studio, the same commands work from a Developer
PowerShell (add `--config Release` to the build and `-C Release` to ctest).

`ctest` runs the unit and scenario tests, the static check that keeps floats
and other nondeterminism out of the core (`tools/check_core_rules.py`), and a
check that the generated rules data is current. The same build adds the live battle
simulation and the trained battle AI ([battle/](../battle/README.md)): `battle_tests`
and the trainer `battle_train`; and the diplomacy dialogue layer
([diplomacy/](../diplomacy/README.md)): `diplomacy_tests` and the `diplo_chat` tool; and
online play ([net/](../net/README.md)): `net_tests` and the `sovnet` tool.

## Headless simulator

```bash
core/build/sovsim --rules data/rules --seed 7 --players 4 --size MAPSIZE_TINY --turns 100 --map
```

Plays every player with a dumb bot (`--ai`: with the AI; `--ai-seats N`: the AI
in the first N seats and the bot in the rest), replays the command log to prove it
reproduces the same state, and prints the state hash. Two machines running the
same command must print the same hash.

## How it fits together

| Piece | Where |
|---|---|
| Fixed-point numbers (no floats anywhere) | `include/sovereign/fixed.h` |
| Seeded RNG streams (map, gameplay, combat, AI, visual) | `include/sovereign/rng.h` |
| Hex grid: pointy-top, odd-row offset, east-west wrap | `include/sovereign/hex.h` |
| Rules data loader with mod layers and checksum | `include/sovereign/rules.h`, data in `../data/rules/` |
| Game state | `include/sovereign/state.h` |
| Commands (the only way state changes) | `include/sovereign/commands.h` |
| Game: submit, replay, paths, visibility, turns | `include/sovereign/game.h` |
| Map generator and start positions | `include/sovereign/mapgen.h` |
| Cities: yields, growth, borders, production, citizens and specialists | `src/city.cpp` (queries on `Game`) |
| Research trees, boosts, governments, anarchy, policy cards | `src/research.cpp` (queries on `Game`) |
| Builders, improvements, harvests, luxuries, strategic stockpiles | `src/improvements.cpp` (queries on `Game`) |
| War and peace, unit and city combat, walls, capture and raze, elimination, ZOC, XP, promotions, healing; pillage and repair; Corps and Armies; the Encampment's strike, zone of control and city strength | `src/combat.cpp` (queries on `Game`); pillage in `src/improvements.cpp` |
| Barbarian player, camps and raiders (acts in the world turn) | `src/barbarians.cpp` |
| City projects (03: Projects): district projects with yield conversion and great person points, one-time effects, the space race chain and the Science victory (light-years) | `src/city.cpp` (`completeProject`, costs and rules), data in `../data/rules/projects.json` |
| Civ identities (leaders-and-art-style): unique units, buildings and improvements on a `base`, civ and leader abilities (modifiers plus typed `CivAbility` fields) | rules loading in `src/rules.cpp` (`Json::overlay`, `uniqueUnitFor`, `combined`), hooks across `src/city.cpp`, `src/combat.cpp`, `src/districts.cpp`, `src/improvements.cpp`, `src/trade.cpp`; data in `../data/rules/civilizations.json` |
| Player profiles (leader doc §10 player modelling): army mix, militarism, expansion, yield leaning, aggression, leader exposure, live-battle habits, per major civ each world turn; carried between games through the setup and a text file | `src/profile.cpp` (`Game::profile`), `profileToText` / `profileFromText` in `src/serialize.cpp`; read by `src/ai.cpp` counters |
| Difficulty levels (00-overview: Difficulty levels; Sovereign: AI bonuses only at Immortal and Deity) | `Game::difficulty`, hooks in `src/city.cpp`, `src/combat.cpp`, `src/eras.cpp`, `src/barbarians.cpp`, `Game::create`; data in `../data/rules/setup.json` |
| Air power (05: air units, air combat): bases and air slots (cities, Aerodromes, Aircraft Carriers, Airstrips), rebasing, air strikes, interception and anti-air; Military Engineer improvements (Fort, Airstrip, Missile Silo) | `src/air.cpp`; strikes through `src/combat.cpp` |
| Nuclear weapons (05: Nuclear weapons): Manhattan Project and Operation Ivy, Nuclear and Thermonuclear Devices with their upkeep, delivery by bombers, Nuclear Submarines and Missile Silos, blast and fallout; the AI keeps a deterrent and answers in kind | `src/wmd.cpp`; `wmds` in `data/rules/projects.json`; AI `nuclear` in `src/ai.cpp` |
| Unit upgrades (05: Upgrades): cost, rules, the command | `src/combat.cpp` (`upgradeCost`, `upgradeProblem`), applied in `src/game.cpp` |
| Districts: placement (Aqueduct, Dam, Canal, flat-land districts, exclusive and one-per-player rules), population limit, cost, adjacency, appeal, housing and amenities from Aqueduct, Neighborhood, Entertainment Complex, Water Park, Dam, Preserve | `src/districts.cpp` (queries on `Game`) |
| The AI player: diplomacy, research, production, settling, armies (commands only) | `include/sovereign/ai.h`, `src/ai.cpp` |
| Score, turn limit and victories (Domination, last standing, Score) | `src/victory.cpp` (queries on `Game`) |
| The leader: own layer, tech-gated gear, escorts, capture, barbarian safety, linked moves, succession and regicide, assassins (off-map agents), SOVEREIGN promotions and the presence aura | `src/leader.cpp` (queries on `Game`), data in `../data/rules/leader.json` |
| Naval movement and embarkation (ships on Coast, Ocean after Cartography; land units embark after Shipbuilding) | `src/game.cpp` (`terrainCost`), `src/combat.cpp`, `src/city.cpp` |
| Eras and ages (moments, the world era, Dark/Golden/Heroic Ages), tourism and the culture victory | `src/eras.cpp`, data in `../data/rules/moments.json` |
| City-states: placement at the start, envoys (meeting, civics, influence), tier bonuses, suzerains | `src/citystates.cpp`, data in `../data/rules/citystates.json` |
| Grievances (from wars, captures, razing, denunciations, caught spies, held cities; decay by era), Diplomatic Favor, the World Congress (sessions, votes bought with favor, seven resolutions with effects), scored competitions, the Diplomatic Victory | `src/worldcongress.cpp`, `src/competitions.cpp`, data in `../data/rules/worldcongress.json` |
| Power [GS]: city demand from buildings, free sources (Hydroelectric Dam, renewables), plants burning fuel for cities within 6 tiles, powered bonuses and the shortfall penalty | `src/climate.cpp` (`burnPower`), effects in `src/city.cpp` |
| Climate and natural disasters: CO2 from fuel burned by units and power plants, climate phases I-VII, polar ice and sea level over coastal lowlands (Flood Barrier), floods, eruptions, blizzards, dust storms, tornadoes, hurricanes, droughts and fires with damage and fertility, the favor cost of emissions (`sovsim --disasters N`) | `src/climate.cpp`, data in `../data/rules/disasters.json` |
| Espionage: spy capacity from civics and techs, training, travel, operations (Counterspy, Listening Post, Gain Sources, Siphon Funds, Steal Tech Boost, Sabotage Production, Neutralize Governor, Foment Unrest, Great Work Heist, Recruit Partisans, Breach Dam, Disrupt Rocketry, Fabricate Scandal), spy promotions, the 3d6 ladder with levels and counterspies, escape and capture | `src/espionage.cpp`, data in `../data/rules/espionage.json` |
| Governors: titles from civics, appointing, promoting, assigning and establishing, loyalty, promotion effects through the modifier model (ModSource::Governor), Amani in city-states | `src/governors.cpp`, data in `../data/rules/governors.json`, effects in `../data/rules/modifiers.json` (GOVERNOR_PROMOTION_* sources) |
| Diplomacy: opinion reasons and memories, leader agendas, relationship states, deals (gold, gold per turn, luxuries, strategics, open borders, friendship, peace), denouncing, formal and surprise wars, war weariness, alliances (types, points, levels and their effects; call to arms), emergencies (Military, City-State, Religious, Nuclear, Betrayal), deal text | `src/diplomacy.cpp`, agendas in `../data/rules/civilizations.json`; AI proposals in `src/ai.cpp` |
| World wonders: placement, once in the world, the rival refund, completion effects (wonders load as buildings flagged `wonder`) | `src/wonders.cpp`, data in `../data/rules/wonders.json` |
| Trade routes (capacity, yields by destination district, range, length, plunder) and roads (era tiers, movement, bridges); railroads and Mountain Tunnels by Military Engineers | `src/trade.cpp`, road movement in `src/game.cpp` (`terrainCost`) |
| Religion: pantheons, founding with a Great Prophet, beliefs (modifiers with `BELIEF_*` sources), pressure and followers, Faith purchases, spread, theological combat, founder yields, religious victory | `src/religion.cpp` (religion queries in `include/sovereign/modifiers.h`), data in `../data/rules/religion.json` |
| Great people and Great Works: points from districts and buildings, the shared per-class timeline, recruitment, patronage, activation effects, General and Admiral auras, Great Works in building slots | `src/greatpeople.cpp`, data in `../data/rules/greatpeople.json` |
| Loyalty, citizen pressure, loyalty levels, revolts to the Free Cities and flips back [R&F] | `src/loyalty.cpp` (queries on `Game`) |
| Live battle contract: pending battle, expected result, BattleResult clamped to the band, auto-resolve | `src/combat.cpp` (`PendingBattle` in `include/sovereign/state.h`) |
| Modifier evaluation | `include/sovereign/modifiers.h` |
| Versioned saves | `include/sovereign/serialize.h` |
| `SOV_API` export marker (empty here; dllexport when Unreal loads the core as a DLL) | `include/sovereign/api.h` |

Rules data: `data/rules/{globals,terrain,resources,promotions,units,buildings,districts,barbarians,techs,civics,governments,policies,improvements}.json` are generated
from `specs/civ6/data` by `python3 tools/rules_gen/gen_rules.py`; never edit
them by hand. `civilizations.json`, `leader.json`, `setup.json` and
`modifiers.json` are Sovereign's own and hand-written. Pass more `--rules` directories to layer mods on top (rows
replace by `id`; `"delete": true` removes one).

If you change rules or the save format on purpose, the golden test fails;
regenerate it with `SOVEREIGN_UPDATE_GOLDEN=1 core/build/sovereign_tests golden`
(and bump `kSaveVersion` when the save layout changed).
