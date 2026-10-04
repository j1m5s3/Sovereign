# Unreal bridge, build-plan step 2 (a plain hex map driven by the core)

status: ACTIVE · slice: unreal-bridge · base: a282156 · created: 2026-10-05 · updated: 2026-10-05

Build-plan step 2 of `specs/sovereign/engine-and-architecture.md`: an Unreal Engine 5 project in `unreal/` that compiles the rules core as a module and shows a plain hex map driven by it. The human plays seat 0 with mouse and keyboard; `sov::ai::playTurn` plays every other seat. Each milestone is one PR from `claude/project-thread-zkdmmz`, merged to `main` once CI is green. Out of scope: the leader character (step 3), street scenes, live battles, art, world generation from PCG, multiplayer, saves from the UI, packaging.

## Environment (checked 2026-10-05 on James's PC)
- Unreal Engine 5.8.3 (launcher install, `C:\Program Files\Epic Games\UE_5.8`). Read-only: nothing is written under it.
- Visual Studio Community 2026 (18.10) with the Game development with C++ workload, MSVC 14.51, Windows SDK 10.0.26100.
- Build from the command line: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SovereignEditor Win64 Development -Project="C:\source\games\Sovereign\unreal\Sovereign.uproject" -WaitMutex`.
- GitHub CI has no Unreal; it keeps building `core/` standalone and adds a check that the Unreal core module lists every core source.

## Decisions (do not reopen)
- Project at `unreal/Sovereign.uproject`, engine association 5.8, C++ only: no Blueprints, no hand-made `.umap` or `.uasset`. The game runs on the engine's empty `/Engine/Maps/Entry` map and spawns everything from C++; meshes and materials come from engine content (`/Engine/BasicShapes/*`, `BasicShapeMaterial` with its `Color` parameter).
- Two modules:
  - `SovereignCore` (Runtime): the rules core. Its `Private/` holds one wrapper `.cpp` per `core/src/*.cpp` that only `#include`s that file, and its Build.cs adds `core/include` as a public include path. Unity builds and PCHs are off for it (core files share anonymous-namespace names) and exceptions/RTTI stay at engine defaults. `core/` itself gets no Unreal headers, no Unreal macros and no UE-specific files; `tools/check_core_rules.py` keeps enforcing that. `tools/check_unreal_core_module.py` (CTest) fails when the wrapper list and the `sovereign_core` sources in `core/CMakeLists.txt` differ.
  - `SovereignBridge` (Runtime, primary game module): game mode, mirror, input and HUD. Depends on `SovereignCore`, `ProceduralMeshComponent`, Core/CoreUObject/Engine/InputCore.
- One owner of the `sov::Game`: `USovGameSubsystem` (a `UGameInstanceSubsystem`) loads rules from `<repo>/data/rules` (`FPaths::ProjectDir()/../data/rules`), creates the game and is the only place that calls `Game::submit` or `ai::playTurn`. It broadcasts a "state changed" delegate after every accepted command. Rules data are read from the repo, never copied into the project.
- Game setup for now: command-line overrides `-SovSeed=`, `-SovPlayers=`, `-SovSize=`; defaults seed 7, 4 players, `MAPSIZE_TINY`, standard speed, barbarians on. Seat 0 is human (`PlayerSetup.human`), the rest are AI.
- The mirror shows only what seat 0 knows (world doc: the 3D world is built from the viewing player's knowledge). Unrevealed plots are not drawn; revealed-but-not-visible plots are drawn darker; other players' units appear only on visible plots; cities appear on revealed plots.
- Hex layout matches `sov::HexGrid`: pointy-top, odd rows shifted east by half a hex, row 0 north. World position: x = size·√3·(col + 0.5·(row odd)), y = size·1.5·row (Unreal +Y points south on screen with the default camera yaw, so row 0 is at the top). Hex size 100 cm. East-west wrap is not drawn yet (map shown once; units still path across the seam as the core allows).
- Terrain mesh: one `UProceduralMeshComponent` with one section per terrain type (a dynamic instance of `BasicShapeMaterial` coloured from a C++ colour table keyed by terrain id, darker copy for fogged plots); features (woods, rainforest, marsh...) tint the plot; hills and mountains are raised. Rivers, resources, improvements, borders and yields are later work.
- Units and cities are markers: units are cones (military) or spheres (civilian) coloured by owner, with an HP bar drawn by the HUD; cities are cubes coloured by owner with name and population drawn by the HUD. Markers are pooled and repositioned on every state change (full re-sync; the map is small).
- Input: player actions leave the bridge only as `sov::Command` through the subsystem. Left click selects a unit or city of seat 0; right click on a plot moves (`MoveUnit`), attacks (`Attack` / `RangedAttack`, chosen by `previewAttack`) or strikes from a selected city (`CityStrike`). Keys: `F` found city, `Space`/`Enter` end turn, `.` next unit needing orders, `K` skip, `G` fortify/sleep, `P` production chooser, `T` research chooser, `C` civics chooser, `1`-`9` pick in an open chooser, `Esc` closes it. WASD/arrows pan, mouse wheel zooms. Raw key polling in the player controller (no Enhanced Input assets needed).
- A refused `EndTurn` opens the chooser that unblocks it (production for the first city needing it, research, civics) or jumps to the first unit needing orders; the refusal reason is shown on the HUD.
- AI seats: after seat 0 ends its turn the subsystem runs `ai::playTurn` one seat per frame until it is seat 0's turn again or the game is won (guard: a seat that does not advance is reported and stops the loop).
- HUD is `AHUD` canvas drawing (no UMG assets): turn, gold, science/culture per turn, current research and civic, selected unit/city details, chooser list, last message, winner.
- Writes stay in the repo: the project's `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/` are git-ignored. The engine's own caches (UBT logs, the shared derived-data cache) stay at their defaults under `%LOCALAPPDATA%`; nothing is written under the engine install.

## Pointers
- [specs/sovereign/engine-and-architecture.md : L13-L28] — layers and rules the bridge must keep
- [core/include/sovereign/game.h] — `Game` queries and `submit`
- [core/include/sovereign/commands.h] — every command the bridge may send
- [core/include/sovereign/state.h] — what gets mirrored (plots, units, cities, visibility)
- [core/include/sovereign/hex.h] — offset layout the mirror must match
- [core/include/sovereign/ai.h] — `ai::playTurn` for the other seats
- [core/tools/sovsim.cpp] — reference for loading rules and creating a game

## Micro-steps
<!-- [ ] pending · [>] active · [x] done · [-] dropped (reason) · [!] blocked -->
1. [x] **Build** UE project and `SovereignCore` module — `unreal/Sovereign.uproject`, `Config/`, `Source/Sovereign*.Target.cs`, `SovereignCore` wrappers, empty `SovereignBridge` primary module, `.gitignore`, `tools/check_unreal_core_module.py` in CTest; any core source tweaks the Unreal compiler demands stay plain C++. Done when `SovereignEditor Win64 Development` builds with UBT and CTest is green. PR "Unreal bridge milestone 1". — done 2026-10-05: all 18 core sources compile unchanged in `SovereignCore` (no warnings), `SovereignEditor` builds in ~50 s.
2. [>] **Build** the mirror — subsystem owning the game, game mode on `/Engine/Maps/Entry`, terrain mesh with fog, unit and city markers, camera pawn, HUD basics; all seats AI-played ("spectate seat 0") to prove the mirror follows the core. Unreal automation test `Sovereign.Bridge.*` (headless, `-nullrhi`): the mirror's tile count equals seat 0's revealed plots, and marker counts equal visible units and revealed cities after 20 AI turns. Done with a screenshot of a running map. PR "Unreal bridge milestone 2".
3. [ ] **Build** input and AI seats — selection, right-click move/attack/strike, found city, end turn, choosers, refusal handling, AI seats 1..N. Automation test: scripted commands through the subsystem found a city, set production and research and end 10 turns with the AI playing the rest; replaying the log gives the same state hash. Done with a screenshot of a played turn. PR "Unreal bridge milestone 3".
4. [ ] **Close** slice — `unreal/README.md` (build, run, controls), JIT index and engine-doc build-plan status updated, review against Acceptance, plan archived.

## Acceptance
- `core/` still contains no Unreal header or macro and still builds standalone in CI (GCC, Clang, MSVC); CTest green, including the module-list check.
- `SovereignEditor Win64 Development` builds on James's PC with UE 5.8.3 from the command line.
- Running the project shows the seat-0 view of a core-generated hex map with terrain colours, fog, units and cities.
- The human can play turns with mouse and keyboard; every action is a `sov::Command` through `Game::submit`; AI seats play themselves; the command log replays to the same hash.

## Open questions & risks
- UE 5.8 compiles with C++20 and its own warning set; core code written for C++17 /W4 may trip new warnings (fix in plain C++, or relax warnings for the wrapper module only).
- Engine macros (`check`, `verify`, `TEXT`...) could collide with core identifiers in bridge files that include both; keep core includes before Unreal ones or isolate them.
- Rules are read from `../data/rules` beside the project; packaging will need to stage that folder (later step).

## Changelog
- 2026-10-05 CREATED — 4 steps from build-plan step 2; step 1 started in the same session.
- 2026-10-05 STEP 1 DONE — core compiled as a module with no source changes; `check_core_rules.py` now also bans Unreal macros and more Unreal include roots; step 2 (mirror) started.
