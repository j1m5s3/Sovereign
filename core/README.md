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
check that the generated rules data is current.

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
| Cities: yields, growth, borders, production | `src/city.cpp` (queries on `Game`) |
| Research trees, boosts, governments, anarchy, policy cards | `src/research.cpp` (queries on `Game`) |
| Builders, improvements, harvests, luxuries, strategic stockpiles | `src/improvements.cpp` (queries on `Game`) |
| War and peace, unit and city combat, walls, capture and raze, elimination, ZOC, XP, promotions, healing | `src/combat.cpp` (queries on `Game`) |
| Barbarian player, camps and raiders (acts in the world turn) | `src/barbarians.cpp` |
| Districts: placement, population limit, cost, adjacency | `src/districts.cpp` (queries on `Game`) |
| The AI player: diplomacy, research, production, settling, armies (commands only) | `include/sovereign/ai.h`, `src/ai.cpp` |
| Modifier evaluation | `include/sovereign/modifiers.h` |
| Versioned saves | `include/sovereign/serialize.h` |

Rules data: `data/rules/{globals,terrain,resources,promotions,units,buildings,districts,barbarians,techs,civics,governments,policies,improvements}.json` are generated
from `specs/civ6/data` by `python3 tools/rules_gen/gen_rules.py`; never edit
them by hand. `civilizations.json`, `setup.json` and
`modifiers.json` are Sovereign's own and hand-written. Pass more `--rules` directories to layer mods on top (rows
replace by `id`; `"delete": true` removes one).

If you change rules or the save format on purpose, the golden test fails;
regenerate it with `SOVEREIGN_UPDATE_GOLDEN=1 core/build/sovereign_tests golden`
(and bump `kSaveVersion` when the save layout changed).
