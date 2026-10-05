# Sovereign Unreal project

The Unreal Engine 5.8 front end (build-plan step 2 of
[engine-and-architecture.md](../specs/sovereign/engine-and-architecture.md)): a plain
hex map driven by the rules core in [../core](../core). The core decides everything;
Unreal draws what seat 0 knows and sends player orders back as `sov::Command`.

## Modules

| Module | What it is |
|---|---|
| `SovereignCore` | The rules core. `Private/Core/SovCore_*.cpp` are generated one-line wrappers that `#include` each `core/src/*.cpp`, so `core/` holds no Unreal file. Regenerate them with `python tools/check_unreal_core_module.py --write` after adding a core source (CTest fails until you do). |
| `SovereignBridge` | The primary game module: `FSovSession` / `USovGameSubsystem` own the `sov::Game` and step AI seats with `sov::ai::playTurn`; `BuildMirror` flattens seat 0's knowledge; `ASovMapActor` draws it; `ASovPlayerController` turns input into commands; `ASovHUD` draws the canvas HUD. |

The editor loads each module as a DLL, so the core's public API is marked `SOV_API`
(`core/include/sovereign/api.h`, empty in the standalone build). Add it to new public
classes and free functions in core headers; the Unreal link step names any you miss.

There are no `.uasset` or `.umap` files: the game runs on the engine's empty
`/Engine/Maps/Entry` map, and `ASovGameMode` spawns the light, sky and map from C++
using engine basic shapes.

## Build

Needs Unreal Engine 5.8 and Visual Studio 2026 (or 2022) with the Game development with
C++ workload.

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" SovereignEditor Win64 Development -Project="C:/source/games/Sovereign/unreal/Sovereign.uproject" -WaitMutex
```

Or right-click `Sovereign.uproject` > Generate Visual Studio project files and build
`SovereignEditor` from the IDE.

## Run

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/source/games/Sovereign/unreal/Sovereign.uproject" -game -windowed -ResX=1600 -ResY=900
```

Options: `-SovBattleDemo` (developer start: your warrior on your leader's plot and an enemy warrior next to it, at war, to try a live battle at once), `-SovNavalDemo` (developer start: Shipbuilding, a galley and an embarked warrior on the coast nearest your leader), `-SovSeed=N` (default 7), `-SovPlayers=N` (default 4), `-SovSize=MAPSIZE_TINY`,
`-SovSpectate` (the AI plays every seat while you watch seat 0's view). Rules are read
from `../data/rules`.

## Controls

| Input | Action |
|---|---|
| Left click | Select your unit or city (click again to cycle units, then the city) |
| Right click | Selected unit: move, or attack an enemy (ranged if it can). Selected city: city strike |
| `F` | Found a city with the selected settler (opens the production chooser) |
| `K` / `G` | Skip the unit this turn / fortify (military) or sleep (civilian) |
| `B` | Builder: choose an improvement or harvest here |
| `E` | Leader (in your city): change weapon, armor or mount (costs gold and the leader's turn) |
| `L` | Link the leader and the military unit on its plot as escort (they move together), or release it |
| `H` | The throne: choose a successor after the leader falls (an heir may keep one promotion), or abandon a captured leader |
| `U` | Promote the selected unit or leader (the leader has three branches; only one can be finished per reign) |
| `Q` | Leader in one of your cities: walk its City Center at street level (autosaves first). WASD walk, hold right mouse or Q/E to look, `F` talks to the herald (Benevolence) or the captain of the guard (Fear), `Esc` returns to the map |
| `V` / `X` | City panel with the leader in that city: Benevolence / Fear without walking (classic control) |
| `B` / `R` | A melee involving your leader's stack waits for you (even on an AI's turn): `B` fights it as a live medieval battle (autosaves first), `R` auto-resolves it. In battle: WASD move the leader, left click or `F` strike, `Tab` charge or hold your men, `1`-`6` order your squads (advance, hold, flank left, flank right, fall back, hunt their leader; `7` `8` `9` pick the left, centre or right squad, `0` all), hold right mouse or Q/E to look, `Esc` settles it now. The trained battle AI (`data/battle_ai`) leads the enemy. The field result goes to the core, which keeps it within 25% of the expected Civ result |
| `J` | Assassins: send an idle one after a rival ruler (shows the odds when that ruler is in sight), or recall one |
| `P` / `T` / `C` | Production / research / civics chooser; `1`-`9` picks, `0` next page, `Esc` closes |
| `.` | Next unit that needs orders |
| `Space` / `Enter` | End turn. If the core refuses, the HUD shows why and opens what is needed |
| `WASD` / arrows, wheel, `Home` | Pan, zoom, back to your capital |

## Tests

Headless automation tests (no window):

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/source/games/Sovereign/unreal/Sovereign.uproject" -ExecCmds="Automation RunTests Sovereign; Quit" -nullrhi -unattended -nosplash
```

- `Sovereign.Bridge.HexLayoutRoundTrip`: world positions and picking match `sov::HexGrid`.
- `Sovereign.Bridge.MirrorFollowsCore`: after 20 all-AI turns, the mirror's tiles, units and cities equal what seat 0 knows.
- `Sovereign.Bridge.LeaderInMirrorAndCommands`: seat 0's leader appears as a leader marker with its ruler's name, and the escort link goes through as a command.
- `Sovereign.Street.CityCenterFromGameState`: the generated City Center has a landmark per building (the Palace included), houses and crowd by population, six streets, no walls without wall buildings, and the same layout for the same hex.
- `Sovereign.Battle.NumbersDecideMostFights`: the battle simulation hurts both sides in even fights, lets a much stronger side win and lose less in at least 10 of 12 seeds, repeats itself with no input, and handles an unescorted leader.
- `Sovereign.Battle.TrainedCommanderLeads`: the trained battle AI loads from `data/battle_ai/commander.txt`, leads the enemy, the human orders one squad without touching the others, and the battle ends within HP bounds.
- `Sovereign.Bridge.HumanSeatPlaysThroughCommands`: a scripted seat 0 founds a city and plays 10 turns through commands with AI opponents; the log replays to the same state hash.

GitHub CI has no Unreal; it builds the core standalone and checks the wrapper list.

## Not yet

Street scenes use engine primitives (one temperate kit) until the art pipeline exists. East-west wrap is not drawn (the map is shown once), and there are no rivers, resources,
improvements, borders, yields, promotions UI, diplomacy UI or saves in the UI yet.
