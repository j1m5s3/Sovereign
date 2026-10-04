# Sovereign: engine and architecture

Status: decided by James (2026-10-04). Companion to [world-scale-and-generation.md](world-scale-and-generation.md), [leader-character-brainstorm.md](leader-character-brainstorm.md) and [leaders-and-art-style.md](leaders-and-art-style.md).

## Decisions

- **[decided, James 2026-10-04] Engine: Unreal Engine 5.** It ships most of what Sovereign's twist needs: World Partition streaming for the ~84 × 54 km world, PCG for building it from hex state at runtime, Nanite for distant detail, a mature character/animation stack for leader fights and real-time battles, and Mass for street crowds. Licensing: free until $1M revenue, then 5% royalty.
- **[decided, James 2026-10-04] Hybrid architecture: a separate rules core.** All Civ-style game rules live in a plain C++ library that never includes Unreal headers. Unreal renders and plays the world from the core's state and sends player actions back to it.

- **[decided, James 2026-10-04] Combat destruction: Chaos, scoped and cosmetic.** See [Destruction](#destruction).
- **[decided, James 2026-10-04] Core foundations and build plan** from the gap review. See [Core foundations](#core-foundations-decided-james-2026-10-04-gap-review) and [Build plan](#build-plan-decided-james-2026-10-04-gap-review).

## Layers

| Layer | Owns | Does not own |
|---|---|---|
| **Rules core** (plain C++, no Unreal dependency) | Game state, turn processing, yields, tech/civics, cities, combat math, diplomacy, strategic AI, save format, rules data loading | Rendering, input, audio, animation |
| **Bridge** (Unreal module) | Mirrors core state into actors (units, cities, borders, yields); turns player input into core actions; feeds hex state to the world generator | Any game rule |
| **Unreal presentation** | World generation and streaming, rendering, UI, street scenes, real-time battles, audio | Deciding outcomes outside live scenes |

## Rules

- **The core never includes Unreal headers.** It is a module inside the Unreal project and also builds standalone (CMake), so it is one codebase, not two projects.
- **Deterministic simulation.** Same state plus same actions gives the same result on every machine: seeded RNG owned by the core, no engine floating-point in rules, no dependence on frame timing. This is what multiplayer sync relies on (see the multiplayer notes in [world-scale-and-generation.md](world-scale-and-generation.md)).
- **Headless by design.** Tests, balance runs and battle-AI self-play run the core without the editor.
- **Rules are data.** Units, buildings, techs, etc. load from data files (shaped like the tables in `specs/civ6/data/`), so mods change data rather than Unreal assets.
- **Live scenes report back.** Real-time battles, assassination encounters and street interactions run in Unreal and hand a result (casualties, leader wounds, happiness/fear change) back to the core, which applies it. The core can also auto-resolve the same scene when no one is controlling it.
- **Third-party runtimes** (llama.cpp for diplomacy, ONNX Runtime for battle AI) sit behind small interfaces so they can be swapped or stubbed in tests.

## Core foundations **[decided, James 2026-10-04, gap review]**

Pinned before the first line of the core is written (see [open-gaps-review.md](open-gaps-review.md), gaps 1, 3, 9, 10 and 18).

- **Numbers:** fixed-point or integer math for every rules value. No floats in the core.
- **Modifiers:** Civ VI's modifier system (collection, effect, owner and subject requirement sets, arguments; `specs/civ6/00-overview.md`, "Architecture recommendations for a clone") is the one way to express abilities, policies, beliefs, gear, promotions, reputation and difficulty.
- **Commands:** every change to game state is a command, whoever issues it (player, AI, network, live-scene result, language-model output), and every command goes into one log. That log serves multiplayer, replays, tests and the weekly challenge.
- **Non-deterministic parts are inputs, never computation.** The language model, the ONNX battle AI, Chaos physics and live scenes run on one machine and enter the core as recorded commands. Replays replay the commands; nothing re-runs them.
- **Live scenes online:** one machine hosts each live battle or assassin fight with Unreal's networking for that scene only, then sends one result command into the lockstep stream. The core checks it against the battle result band (leader doc section 9) before applying it.
- **RNG:** separate seeded streams for map generation, combat, AI and world visuals, so a cosmetic draw never shifts a combat roll.
- **Saves:** versioned format from day one, with golden-file round-trip tests (`specs/civ6/10-ai-ui-implementation.md`, "Testing strategy"). The game autosaves when a live scene starts; there is no saving inside one.
- **Online services **[decided, James 2026-10-04]**:** Steamworks first (Steam is the main store), with Epic Online Services added for cross-store play.
- **Platforms:** Windows PC first, mouse and keyboard plus controller for direct control, Steam Deck as the low-end test device. Consoles later.

## Build plan **[decided, James 2026-10-04, gap review]**

Written up as the first `.jit/JIT_PLAN.md` when coding starts:

1. Headless Civ core through MVP-7 (`specs/civ6/10-ai-ui-implementation.md`, "Recommended build order"), with tests. *Done 2026-10-05 (`core/`).*
2. Unreal bridge showing a plain hex map. *Done 2026-10-05 (`unreal/`, UE 5.8): seat 0 plays with mouse and keyboard through commands, the AI plays the other seats.*
3. The leader in classic control, with auto-resolved assassins.
4. One street scene: one style, the City Center.
5. One medieval live battle.
6. Everything else.

Art **[decided, James 2026-10-04]**: Claude makes the art, modelling from reference images found online (see [leaders-and-art-style.md](leaders-and-art-style.md), Art toolchain).

## Destruction

Uses Unreal's Chaos destruction, mainly in combat. Cost scales with how many pieces are simulating at once, not with how many things can break.

- **The core decides, Chaos shows it.** Breached walls, damaged or pillaged buildings and destroyed siege engines are rules-core state. Chaos only plays the collapse. Physics results differ between machines, so they never decide outcomes.
- **Break only what matters in a fight:** walls, gates, towers, siege engines and buildings inside the battle area. Background and residential pieces stay solid.
- **Recorded collapses for set pieces.** Big moments (a wall section coming down) replay a pre-recorded Chaos simulation: same look everywhere, a fraction of the cost of live physics.
- **Short-lived debris.** Debris falls, settles, then is swapped for pre-made "ruined" kit pieces, keeping the physics load low alongside hundreds of soldiers.
- **Damage persists.** After a battle the core records what was damaged; the world generator builds those hexes from damaged variants until repaired.
- **Multiplayer:** each client runs the cosmetic physics locally; only core state is synced.
- **Climate disasters** (earthquakes, eruptions, floods) reuse the same system when they hit a city.

Costs: fracture setups and ruined variants for the breakable subset of each kit (not all ~30 kits in full); profile soldiers + physics + Nanite early; destruction detail is the first setting lowered on weak PCs.

## Known costs (accepted)

- A bridge layer to write and maintain for every visible system.
- No Blueprints, reflection or built-in save/replication for game logic; the core has its own types and save format.
- Two build setups (inside Unreal and standalone CMake).
- Bugs can sit in the core, the bridge or the presentation.

## Risks to plan for

- Dense 4X screens (city panels, tech tree, trade) are slow to build in Unreal's UI tools; budget real time for UI.
- Lumen and Nanite raise the minimum PC spec; the stylized art lets us drop Lumen if needed.
- Weaker Mac and low-end support than Unity or Godot.
- Long C++ compile times and a steep learning curve.
