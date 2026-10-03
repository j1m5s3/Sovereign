# Civilization VI Feature Specification: Overview and Index

Purpose: a complete, implementation-oriented description of the features of Sid Meier's Civilization VI (base game plus the Rise and Fall and Gathering Storm expansions), written so an engineer or AI agent can build a functional clone. Numbers in this spec have been checked against the game's own rules database (Gathering Storm ruleset with all overrides applied; see [`data/`](data/README.md)). Mechanics that live in the engine rather than in data are marked `(engine; source: ...)` or `(engine; unverified)`. A clone should load all values from data files, not hard-code them.

Legend used across files:
- **[Base]** base game, **[R&F]** Rise and Fall, **[GS]** Gathering Storm.
- **MVP** marks the minimum set needed for a playable 4X loop.
- Yields: Food (F), Production (P), Gold (G), Science (S), Culture (C), Faith (Fa), Tourism (T).
- `(data: Table.Column)` or `(data: GlobalParameters.NAME)` points to where a value lives in the game database.

## File index

| File | Covers |
|---|---|
| `00-overview.md` | This file: game loop, turn structure, speeds, difficulty, architecture, data model |
| `01-map-and-terrain.md` | Hex grid, terrain, features, resources, rivers, map generation, visibility, appeal |
| `02-cities.md` | Founding, population, food, housing, amenities, loyalty, tiles, production, borders |
| `03-districts-buildings-wonders.md` | Districts, adjacency, buildings, wonders, builders and improvements |
| `04-tech-civics-government.md` | Tech tree, civics tree, boosts, governments, policy cards, eras |
| `05-units-and-combat.md` | Units, movement, ZOC, combat math, promotions, formations, siege, naval, air, nuclear |
| `06-religion.md` | Faith, pantheons, religions, beliefs, religious units, theological combat |
| `07-economy-trade-great-people.md` | Gold, maintenance, trade routes, great people, great works, tourism |
| `08-diplomacy-city-states-governors.md` | Diplomacy, grievances, alliances, city-states, governors, World Congress |
| `09-civs-eras-victory-climate.md` | Civ/leader abilities, era score and ages, victory types, climate and disasters |
| `10-ai-ui-implementation.md` | AI behavior, UI screens, save data, turn processing order, MVP roadmap |
| `data/*.md` | Generated reference tables (every unit, building, tech, civic, policy, belief, leader, modifier effect, global parameter, setup table). Start at [`data/README.md`](data/README.md). Do not edit by hand. |
| `../../tools/civ6_extract/` | The extractor that rebuilds the rules database from a local game install and regenerates `data/*.md` (`build_db.py`, `extract.py`, `effects.py` renders modifiers as plain text). |

## High-level game concept

Turn-based 4X (explore, expand, exploit, exterminate) on a hexagonal tile map. Major civilizations plus city-states and barbarians compete from 4000 BC (data: StartEras.Year = -4000 for an Ancient start) to AD 2050, when the turn limit triggers a Score victory. The default number of major civs depends on map size, from 2 (Duel) to 12 (Huge) (data: Maps.DefaultPlayers; see [game-setup.md](data/game-setup.md#map-sizes)). Each player leads one civilization with one leader; both carry unique bonuses. The ruleset in `data/` has 50 playable civilizations and 77 leaders.

Signature design ideas that distinguish Civ VI from earlier entries and must be present for the clone to "feel" like Civ VI:
1. **Unstacked cities**: cities spread specialized *districts* across map tiles; placement and adjacency matter.
2. **Boosts**: Eurekas (tech) and Inspirations (civic) give 40% of the item's cost for completing an in-game task (data: Boosts.Boost = 40 for 125 of 126 boosts; the Near Future Governance inspiration gives 90). Modifiers raise it: China's Dynastic Cycle +10 (to 50%), Babylon's ability makes a Eureka complete the whole tech, and some Golden Age commemorations add +10%.
3. **Two research trees**: Technology (Science) and Civics (Culture), equal in importance.
4. **Policy-card governments**: slot-based cards in Military, Economic, Diplomatic and Wildcard slots.
5. **Limited stacking**: a tile may hold one land military unit, one naval military unit, one support unit, one civilian unit and one religious unit at the same time; same-type units merge into corps/armies (engine; source: https://civilization.fandom.com/wiki/Unit_(Civ6)). Religious units got their own stacking class in the Fall 2017 update.
6. **Builders with charges** instead of permanent workers.
7. **Housing and Amenities** as soft caps on city growth and happiness.
8. **Agendas**: AI leaders have a visible historical agenda and a hidden random agenda.
9. **Era score, Golden/Dark/Heroic ages, Loyalty, Governors** [R&F].
10. **Climate change, natural disasters, power, World Congress, Diplomatic Victory** [GS].

## Core game loop

```
setup game (map, players, difficulty, speed, victory types)
loop per turn:
    for each player (sequential in single player; simultaneous or dynamic in multiplayer):
        start-of-turn processing (see turn order below)
        player issues orders: move units, choose research/civic, set production,
                              manage citizens, diplomacy, trade, policies, etc.
        end turn (blocked by required decisions, see below)
    World Congress segments when a session is due [GS] (see turn phases)
    check victory conditions
```

### Turn phases and segments (data: TurnPhases, TurnSegments)
The engine runs each turn as a list of *segments* selected by the turn mode. This is data, so a clone can copy it directly:

| Turn phase type | Mode | Segments in order |
|---|---|---|
| Single player | Sequential | Main play segment; then [GS] World Congress part 1, part 2, resolution |
| Simultaneous | Simultaneous | Single segment; then the three World Congress segments |
| Dynamic | Dynamic (simultaneous while at peace, sequential between players at war) | Single segment; then the three World Congress segments |
| Two-phase | Strategic segment (simultaneous; strategic commands only), then tactical segment (dynamic; tactical commands only) | |

Each segment carries turn-timer inputs: base seconds plus per-city and per-unit additions (main/single segments 30 + 10 per city + 5 per unit; strategic 20 + 10 per city; tactical 10 + 5 per unit; each World Congress segment 60). Turn timer options are None, Dynamic and Standard (data: TurnTimers).

### Required end-turn blockers (the "Next Turn" button text)
The complete list of blocking states the game UI handles (engine; source: game UI scripts `Base/Assets/UI/ActionPanel.lua`, `DLC/Expansion1/UI/Replacements/ActionPanel_Expansion1.lua` and `DLC/Expansion2/UI/Replacements/ActionPanel_Expansion2.lua` in the game install):
- Units need orders (a unit with moves left that is not sleeping, fortified, on alert or skipped), and units illegally stacked that must move
- Choose research / Choose civic
- Choose production (any city with an empty queue)
- Fill a newly unlocked policy slot; consider a government change when a new one unlocks
- Decide whether to keep or raze a captured city
- Choose pantheon / found religion / choose beliefs
- Assign an envoy (influence token)
- Claim (or pass on) an earned Great Person
- Choose what to do with an excavated artifact
- Spy decisions: escape route, counter-spy (dragnet) priority
- [R&F] Appoint a governor, a governor opportunity, a governor promotion, an idle governor; decide on a city that wants to flip (disloyal city); an emergency needing a decision; choose a commemoration (dedication) at a new era
- [GS] World Congress session vote, and looking at the congress results
- Unit promotions are not a blocker.

### Game speed (data: GameSpeeds, GameSpeed_Turns; full calendar in [game-setup.md](data/game-setup.md#game-speeds))
The cost multiplier applies to production, research, civic, growth-related and purchase costs and great person thresholds. Turn counts are exact; every speed runs 4000 BC to AD 2050.

| Speed | Cost multiplier | Turns | Civic unlock max/min cost, drop per turn |
|---|---|---|---|
| Online | 50% | 250 | 50/10, 5 |
| Quick | 67% | 330 | 75/15, 10 |
| Standard | 100% | 500 | 100/20, 10 |
| Epic | 150% | 750 | 150/30, 15 |
| Marathon | 300% | 1500 | 300/60, 30 |

Durations measured in turns (timed effects, cooldowns) use two softer scalings (data: GameSpeed_Scalings): *Half* (Online 66%, Quick 87%, Epic 125%, Marathon 200%) and *Slight* (Online 80%, Quick 90%, Epic 110%, Marathon 120%), with explicit lookup rows for common durations in [game-setup.md](data/game-setup.md#game-speed-durations).

### Difficulty levels
Order (index 0 to 7): Settler, Chieftain, Warlord, Prince, King, Emperor, Immortal, Deity. All difficulty effects are ordinary modifiers attached to every major civ (data: TraitModifiers for TRAIT_LEADER_MAJOR_CIV), with arguments of type `LinearScaleFromDefaultHandicap`: value = base + step x (difficulty index - reference index). The reference ("default handicap") is **Prince** (index 3); this matches published per-level values such as King +8% science and Deity +32%/+80% (source: https://gamerant.com/civilization-6-difficulty-levels-explained/, https://civfandom.com/civ-6-game/civ-6-difficulty-levels-guide/).

AI bonuses apply only to AI players at Prince or above (requirement set PLAYER_IS_HIGH_DIFFICULTY_AI). Human bonuses apply only to a human playing Settler or Chieftain (PLAYER_IS_LOW_DIFFICULTY_HUMAN: not at or above Warlord).

| Level | AI Science/Culture/Faith | AI Production/Gold | AI combat strength | AI unit XP | Free AI Eurekas and Inspirations per new era | Human combat strength | Human unit XP | Human gold from barbarian camps | Diplomacy random offset |
|---|---|---|---|---|---|---|---|---|---|
| Settler | - | - | - | - | - | +3 | +45% | +15% | +3 |
| Chieftain | - | - | - | - | - | +2 | +30% | +10% | +2 |
| Warlord | - | - | - | - | - | 0 | 0 | +5% | +1 |
| Prince | 0 | 0 | -1 (see note) | 0 | 0 | - | - | 0 | 0 |
| King | +8% | +20% | 0 (see note) | +10% | 1 each | - | - | -5% | -1 |
| Emperor | +16% | +40% | +1 (see note) | +20% | 2 each | - | - | -10% | -2 |
| Immortal | +24% | +60% | +2 (see note) | +30% | 3 each | - | - | -15% | -3 |
| Deity | +32% | +80% | +3 (see note) | +40% | 4 each | - | - | -20% | -4 |

Notes:
- Yield bonuses are percentage modifiers on city yields; combat strength applies in all of that player's combats (versus humans, other AIs and barbarians).
- **Combat strength discrepancy**: the data row is base -1, step +1, which literally evaluates to -1 at Prince and +3 at Deity. Most guides instead quote +1 at King rising to +4 at Deity (source: https://gamerant.com/civilization-6-difficulty-levels-explained/). Which one the engine actually applies is unresolved (engine; unverified). A clone should keep the formula in data and pick one. Likewise the data gives a Warlord human no combat or XP bonus, while some guides list +1 at Warlord.
- Human loyalty: martial law (garrison) loyalty is adjusted +7 for a human on Settler or Chieftain and -3 for a human on Emperor or above (data: LOW/HIGH_DIFFICULTY_HUMAN_MARTIAL_LAW).
- The diplomacy random offset shifts the random part of AI opinion of every met major civ (data: STANDARD_DIPLOMACY_RANDOM, DifficultyOffset); higher difficulty makes AIs less friendly.
- Barbarians: camps spawn 1 tile further away per difficulty level below the reference and spawn throttling is lowered by 3 per level (data: GlobalParameters.BARBARIAN_CAMP_EXTRA_DISTANCE_PER_LOW_DIFFICULTY, BARBARIAN_LOWER_THROTTLE_PER_DIFFICULTY). See [barbarians-goody-huts.md](data/barbarians-goody-huts.md).

**AI starting units** (data: MajorStartingUnits rows with AiOnly; quantity = floor(Quantity + DifficultyDelta x (difficulty index - MinDifficulty index)); the rounding is engine behaviour, confirmed by the published totals at https://gamerant.com/civilization-6-difficulty-levels-explained/). Every major civ starts with 1 Settler and 1 Warrior in an Ancient start; AIs additionally get:

| Level | Extra Warriors | Extra Builders | Extra Settlers | AI total at start |
|---|---|---|---|---|
| Prince and below | 0 | 0 | 0 | 1 Settler, 1 Warrior |
| King | 1 | 1 | 0 | 1 Settler, 2 Warriors, 1 Builder |
| Emperor | 2 | 1 | 1 | 2 Settlers, 3 Warriors, 1 Builder |
| Immortal | 3 | 2 | 1 | 2 Settlers, 4 Warriors, 2 Builders |
| Deity | 4 | 2 | 2 | 3 Settlers, 5 Warriors, 2 Builders |

In later-era starts the extra Warrior is replaced by the era's defensive unit (Spearman, Pikeman, Pike and Shot, Ranger, AT Crew). City-states get 1 extra Warrior at Emperor, 2 at Immortal and 3 at Deity, and start with Ancient Walls from Immortal up. Full tables: [game-setup.md](data/game-setup.md#starting-units-major-civs).

AI behaviour also changes slightly with difficulty in data: AIs pursue Eureka triggers only from Warlord up, and the Rapid Expansion strategy is disqualified at Warlord or below (see `10-ai-ui-implementation.md`).

### Score (data: ScoringLineItems; see [game-setup.md](data/game-setup.md#scoring))
Score decides the winner at the turn limit (Score victory) and breaks ties. Each line item is a count times a multiplier: civics 3, technologies 2, wonders 15 (these three are flagged ScaleByCost; the exact cost scaling is engine, unverified), cities 5, districts 2, population 1, great people 5, religion (beliefs) 5, era score 1, era buildings 1, converted citizens 2. Trade, pillage and income categories exist but have multiplier 0.

## Architecture recommendations for a clone

- **Data-driven**: every civ, leader, unit, building, district, wonder, tech, civic, policy, belief, resource, improvement, feature, terrain, promotion, great person, project, and modifier lives in data (JSON/YAML/SQL). Civ VI itself uses a SQL-backed "Modifiers" system: a modifier = `(type -> (collection, effect), owner requirement set, subject requirement set, arguments)` (data: Modifiers, DynamicModifiers, ModifierArguments, RequirementSets). Replicating this is the single most important architectural decision, because almost every unique ability, policy card, belief, great person and even difficulty bonus is expressed as a modifier.
- **Modifier system sketch**:
  ```
  Modifier {
    id
    collection: OWNER | PLAYER_CITIES | PLAYER_UNITS | PLAYER_COMBAT | CITY_DISTRICTS | PLOT_ADJACENT ...
    effect:     ADJUST_CITY_YIELD_MODIFIER | ADJUST_PLAYER_STRENGTH_MODIFIER | GRANT_UNIT | ADJUST_MOVEMENT ...
    owner_requirements:   RequirementSet (tested on the object holding the modifier)
    subject_requirements: RequirementSet (tested on each object in the collection,
                                          e.g. "district is Campus", "plot is adjacent to river")
    arguments: { YieldType: SCIENCE, Amount: 2, ... }   // argument values may be typed,
                                                          // e.g. LinearScaleFromDefaultHandicap, ScaleByGameSpeed
    run_once, permanent, new_only: bool
  }
  ```
  Requirement sets combine with ALL/ANY (REQUIREMENTSET_TEST_ALL / TEST_ANY); individual requirements can be inverted. Modifiers attach to an owner (player, city, unit, plot, government, policy, belief) and are recalculated when state changes. `tools/civ6_extract/effects.py` shows how to render any modifier as readable text.
- **Deterministic simulation**: seedable RNG per game (combat variance, map gen, disasters, goody huts). Required for multiplayer lockstep and replay.
- **Separation**: Simulation core (pure state + rules) <- AI agents and UI both issue the same command objects (`MoveUnit`, `SetProduction`, `AdoptPolicy`, ...). This makes AI, network play, and tests share one path.
- **Pathfinding**: A* over hex grid with movement-cost function per unit (terrain, rivers, roads, ZOC, embarkation, borders/open-borders rules).

## Turn processing order (per player, start of turn)
The exact internal order is not documented and is not in data (engine; unverified). The following order is a recommended, consistent implementation:
1. Reset unit movement; heal units that did not act (healing rules in the units file).
2. Accumulate yields from all cities (tiles worked + buildings + districts + specialists + modifiers) and empire-wide sources (trade routes, city-state suzerain bonuses, policies).
3. Gold: add income, subtract building/district/unit maintenance. If the treasury is at 0 and the balance is negative, every city loses 1 Amenity per 10 Gold below 0, and one unit is disbanded at -10 Gold, two at -20, and so on (data: GlobalParameters.GOLD_NEGATIVE_BALANCE_AMENITY_LOSS_LINE = 0, GOLD_NEGATIVE_BALANCE_DISBAND_UNIT_LINE = -10, GOLD_NEGATIVE_BALANCE_SUBSEQUENT_* = -10; behaviour described in the game's own Civilopedia gold page).
4. Science and culture applied to current research/civic; overflow carries. Check completions, apply unlocks.
5. Faith accumulated (check pantheon/religion thresholds, great prophet).
6. Great person points accumulated per class; check if any player reaches the threshold of the next available great person.
7. Each city: food surplus to growth bucket, check growth/starvation; production to current item, check completion; border growth (culture bucket for tiles); loyalty change [R&F]; amenity recalculation.
8. Era score / timeline checks [R&F]; strategic resource stockpiles accumulate/consume [GS]; power calculation [GS].
9. Diplomacy timers (delegations, open borders, alliances, friendships, grievance decay, denouncements).
10. Notifications generated.

After all players: barbarians and city-states act as players of their own; World Congress segments run when a session is due [GS]; climate, sea level and disasters update [GS]; global era advancement is checked [R&F]: each game era lasts between 40 and 60 turns on Standard speed (data: Eras_XP1.GameEraMinimumTurns/GameEraMaximumTurns), and players get a countdown warning 10 turns before it changes (data: GlobalParameters.NEXT_ERA_TURN_COUNTDOWN); the trigger inside that window (civs' own era progress) is engine (engine; unverified); victory checks (see [game-setup.md](data/game-setup.md#victories)).
