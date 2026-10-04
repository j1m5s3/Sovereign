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

Options: `-SovSeed=N` (default 7), `-SovPlayers=N` (default 4), `-SovSize=MAPSIZE_TINY`,
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
- `Sovereign.Bridge.HumanSeatPlaysThroughCommands`: a scripted seat 0 founds a city and plays 10 turns through commands with AI opponents; the log replays to the same state hash.

GitHub CI has no Unreal; it builds the core standalone and checks the wrapper list.

## Not yet

East-west wrap is not drawn (the map is shown once), and there are no rivers, resources,
improvements, borders, yields, promotions UI, diplomacy UI or saves in the UI yet.
