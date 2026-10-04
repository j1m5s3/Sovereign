# Sovereign JIT index

last_verified: 2026-10-04 (against main @ 21a35ed)

Living map of the repo for AI agents. Read this instead of the full specs (~830 KB, ~207k tokens), then open only the files and sections a task needs. Kept current by whoever changes the docs. Active work plans go in `.jit/JIT_PLAN.md` (none yet; development has not started).

## Reading rules
- **Precedence:** `specs/sovereign/` overrides `specs/civ6/` where they differ. Civ VI is the baseline Sovereign clones; Sovereign docs record the deliberate changes.
- **Prose before data:** the numbered `specs/civ6/NN-*.md` files explain rules (3-9k tokens each). `specs/civ6/data/*.md` are generated lookup tables; never read one whole.
- **Look up, don't load:** in a data file, `grep -n '^## '` for its sections, then `grep -n '<name>'` for the row you need (the header row is the first `|` line under the section heading). Big ones: civilizations-leaders (~25k tok), units (~13k), great-people (~12k), governments-policies (~9k).
- **Generated files:** `specs/civ6/data/*.md` come from `tools/civ6_extract`; change the extractor, never the tables.
- **Unverified rules** are tagged `(engine; unverified)` in the prose; grep for it before relying on an engine number.

## Where to look, by topic
| Topic | Rules (prose) | Numbers (data) | Sovereign changes |
|---|---|---|---|
| Game loop, turn order, speeds, difficulty, score | 00-overview: Core game loop; Turn processing order | game-setup | |
| Map, terrain, resources, rivers, map gen | 01-map-and-terrain | terrain-features-resources, improvements | world-scale-and-generation |
| Barbarians, tribal villages | 01: Tribal Villages; Barbarians · 05: Barbarian AI combat | barbarians-goody-huts | |
| Cities: growth, housing, amenities, loyalty, borders | 02-cities | eras-moments-loyalty (Amenities, Loyalty levels), global-parameters (CITY, CITIZEN) | leader-character-brainstorm §4 (interiors) |
| Districts, buildings, wonders, builders, projects | 03-districts-buildings-wonders | districts, buildings, wonders, improvements, projects | world-scale-and-generation (city models) |
| Techs, civics, boosts, governments, policies | 04-tech-civics-government | technologies, civics, governments-policies | leader-character-brainstorm §2 (tech-gated gear) |
| Units, movement, combat math, promotions | 05-units-and-combat | units, promotions, global-parameters (COMBAT, EXPERIENCE) | leader-character-brainstorm §1-3, §9 |
| Religion | 06-religion | religion | |
| Gold, trade, great people, great works, tourism | 07-economy-trade-great-people | great-people, buildings (Great Work slots) | |
| Diplomacy, city-states, governors, espionage, World Congress | 08-diplomacy-city-states-governors | diplomacy-espionage, city-states, governors, world-congress-emergencies | leader-character-brainstorm §6 (assassins) |
| Civs and leaders | 09: Civilization and leader structure | civilizations-leaders (grep the civ name) | leaders-and-art-style (Sovereign's own roster) |
| Eras, ages, victory, climate, power | 09-civs-eras-victory-climate | eras-moments-loyalty, climate-disasters, game-setup (Victories) | |
| AI | 10: AI architecture · 08: Agendas | civilizations-leaders (Agendas) | leader-character-brainstorm §10 |
| UI, saves, multiplayer, build order, testing | 10-ai-ui-implementation | | engine-and-architecture (Core foundations, Build plan) |
| Any tunable constant | | global-parameters (sections are name prefixes, e.g. `## COMBAT`) | |

Paths: prose is `specs/civ6/<file>.md`, data is `specs/civ6/data/<file>.md`, Sovereign docs are `specs/sovereign/<file>.md`.

## Docs
- [specs/civ6/00-overview.md] — entry point to the Civ VI spec: legend, file index, game loop, architecture notes.
- [specs/civ6/data/README.md] — list of the 23 generated tables.
- [specs/sovereign/engine-and-architecture.md] — UE5 presentation + separate engine-independent C++ rules core; rules load from data; Chaos destruction plan.
- [specs/sovereign/leader-character-brainstorm.md] — the playable leader: map presence, gear, levelling, interiors, death and heirs, assassins, AI. Brainstorm with decided sections marked `[decided]`.
- [specs/sovereign/leaders-and-art-style.md] — stylized realism; 12 launch civs of historical leaders.
- [specs/sovereign/world-scale-and-generation.md] — map scale and procedural city/district model generation.
- [specs/sovereign/player-retention.md] — nemesis rivals, reign story, challenges, shorter modes, mods.
- [specs/sovereign/open-gaps-review.md] — gap review; James adopted all recommendations except gap 5 (2026-10-04). Decisions now live in the docs they affect; this file keeps the reasoning.

## Subsystems
- [tools/civ6_extract/] — rebuilds the rules DB from a local Civ VI install and regenerates `specs/civ6/data/` (README.md there for usage; rerun after game patches).

## Decisions
- Clone Civ VI first (base + Rise and Fall + Gathering Storm), then layer the Sovereign twist (playable leader character).
- Engine: UE5 front end, separate C++ rules core with no Unreal dependency.
- Rules are data: load values from data files, never hard-code.
- Leader roster: historical figures only, nobody living or recently dead.
- Core foundations (engine doc): fixed-point math, Civ's modifier system, one command log, non-deterministic parts enter only as recorded commands, versioned saves.
- Live battles (leader doc §9): only melee involving the leader's stack goes live; core computes the Civ result and the scene shifts it within a band; one machine hosts each scene; the leader fights personally in every era.
- Leader ability belongs to the throne; every successor keeps it (leader doc §5).
- The 3D world is generated from what the viewing player knows, not the true state (world doc).
