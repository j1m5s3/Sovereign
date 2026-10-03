# Civilization VI Feature Specification: Overview and Index

Purpose: a complete, implementation-oriented description of the features of Sid Meier's Civilization VI (base game plus the Rise and Fall and Gathering Storm expansions), written so an engineer or AI agent can build a functional clone. Where exact game numbers are given they reflect the shipped game as of its final patches; values marked `~` are approximate and should be treated as tunable defaults rather than canon. A clone should load all such values from data files, not hard-code them.

Legend used across files:
- **[Base]** base game, **[R&F]** Rise and Fall, **[GS]** Gathering Storm.
- **MVP** marks the minimum set needed for a playable 4X loop.
- Yields: Food (F), Production (P), Gold (G), Science (S), Culture (C), Faith (Fa), Tourism (T).

## File index

| File | Covers |
|---|---|
| `00-overview.md` | This file: game loop, turn structure, architecture, data model, build order |
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

## High-level game concept

Turn-based 4X (explore, expand, exploit, exterminate) on a hexagonal tile map. 2 to 12 major civilizations plus city-states and barbarians compete from 4000 BC through the information age. Each player leads one civilization with one leader; both carry unique bonuses. The game ends when a player meets a victory condition or the turn limit is reached (score victory).

Signature design ideas that distinguish Civ VI from earlier entries and must be present for the clone to "feel" like Civ VI:
1. **Unstacked cities**: cities spread specialized *districts* across map tiles; placement and adjacency matter.
2. **Boosts**: Eurekas (tech) and Inspirations (civic) give 40% (50% for some civs) of a research cost for completing in-game tasks.
3. **Two research trees**: Technology (Science) and Civics (Culture), equal in importance.
4. **Policy-card governments**: slot-based cards in Military, Economic, Diplomatic and Wildcard slots.
5. **One unit per tile** (1UPT) per category (one military land, one civilian, one support, one religious-ish allowed to stack with civilian/military rules, see units file), with corps/armies merging.
6. **Builders with charges** instead of permanent workers.
7. **Housing and Amenities** as soft caps on city growth and happiness.
8. **Agendas**: AI leaders have a visible historical agenda and a hidden random agenda.
9. **Era score, Golden/Dark/Heroic ages, Loyalty, Governors** [R&F].
10. **Climate change, natural disasters, power, World Congress, Diplomatic Victory** [GS].

## Core game loop

```
setup game (map, players, difficulty, speed, victory types)
loop per turn:
    for each player (simultaneous or sequential depending on mode):
        start-of-turn processing (see turn order below)
        player issues orders: move units, choose research/civic, set production,
                              manage citizens, diplomacy, trade, policies, etc.
        end turn (blocked by required decisions: research choice, production choice,
                  unit needing orders unless skipped/fortified/sleeping, promotions available optional)
    end-of-round processing (world congress sessions, climate, disasters, era change checks)
    check victory conditions
```

### Required end-turn blockers (the "Next Turn" button text)
- Choose research / Choose civic
- Choose production (any city with an empty queue)
- Units need orders (unit with moves left and not sleeping/fortified/alert/skipped)
- Choose pantheon / found religion / choose beliefs (when available)
- Choose government policies (when a new civic unlocks slots or government changes; free swap that turn)
- Governor title available (optional, non-blocking in final game; a clone may make it non-blocking)
- Promotions available (non-blocking)
- Consider dedication on new era (blocking choice of Golden/Normal/Dark age dedication) [R&F]
- World Congress resolutions vote [GS]

### Game speed multipliers (applied to all costs: production, research, civics, growth, faith purchase, great person points thresholds, etc.)

| Speed | Cost multiplier | Approx. turns |
|---|---|---|
| Online | 50% | 250 |
| Quick | 67% | 330 |
| Standard | 100% | 500 |
| Epic | 150% | 750 |
| Marathon | 300% | 1500 |

### Difficulty levels
Settler, Chieftain, Warlord, Prince (no bonuses), King, Emperor, Immortal, Deity. AI receives flat bonuses scaled per level above Prince: extra starting units (extra Settlers, Warriors, Builders), +combat strength vs. human (~+1 per level above Prince, up to +4 at Deity... AI combat bonus vs barbarians too), and yield bonuses (science, culture, production, gold, faith ~+8% per level above Prince up to ~+40% at Deity). Below Prince the human gets combat bonus vs AI. Make this a data table.

## Architecture recommendations for a clone

- **Data-driven**: every civ, leader, unit, building, district, wonder, tech, civic, policy, belief, resource, improvement, feature, terrain, promotion, great person, project, and modifier lives in data (JSON/YAML/SQL). Civ VI itself uses a SQL-backed "Modifiers" system: a modifier = `(type, collection selector, requirement set, effect, arguments)`. Replicating this is the single most important architectural decision, because almost every unique ability, policy card, belief and great person is expressed as a modifier.
- **Modifier system sketch**:
  ```
  Modifier {
    id
    collection: OWNER | ALL_CITIES | ALL_PLAYER_UNITS | CITY_DISTRICTS | PLOT_ADJACENT ...
    effect:     ADJUST_YIELD | ADJUST_COMBAT_STRENGTH | GRANT_UNIT | ADJUST_MOVEMENT | ADJUST_COST ...
    subject_requirements: RequirementSet (e.g., "district is Campus", "unit is melee class",
                                          "plot is adjacent to river", "player has tech X")
    owner_requirements:  RequirementSet
    arguments: { YieldType: SCIENCE, Amount: 2, ... }
    permanent: bool   // re-evaluated each turn vs applied once
  }
  ```
  Requirement sets combine with AND/OR. Modifiers attach to an owner (player, city, unit, plot, government, policy, belief) and are recalculated when state changes.
- **Deterministic simulation**: seedable RNG per game (combat variance, map gen, disasters, goody huts). Required for multiplayer lockstep and replay.
- **Separation**: Simulation core (pure state + rules) <- AI agents and UI both issue the same command objects (`MoveUnit`, `SetProduction`, `AdoptPolicy`, ...). This makes AI, network play, and tests share one path.
- **Pathfinding**: A* over hex grid with movement-cost function per unit (terrain, rivers, roads, ZOC, embarkation, borders/open-borders rules).

## Turn processing order (per player, start of turn)
1. Reset unit movement; heal units (healing rules in units file); apply attrition (GS: blizzard/snow/desert in some modes, extreme heat for not-adapted).
2. Accumulate yields from all cities (tiles worked + buildings + districts + specialists + modifiers) and empire-wide sources (trade routes, city-states as suzerain, policies).
3. Gold: add income, subtract building/district/unit maintenance. If treasury < 0, disband units each turn until balanced (and lose amenities-equivalent penalties).
4. Science and culture applied to current research/civic; overflow carries. Check completions, apply unlocks.
5. Faith accumulated (check pantheon/religion thresholds, great prophet).
6. Great person points accumulated per class; check if any player reaches threshold of the next available GP.
7. Each city: food surplus to growth bucket, check growth/starvation; production to current item, check completion; border growth (culture bucket for tiles); loyalty change; amenity recalculation.
8. Era score / timeline checks [R&F]; resource stockpiles accumulate/consume [GS]; power calculation [GS].
9. Diplomacy timers (delegations, open borders, alliances, friendships, grievance decay, denouncements).
10. Notifications generated.

End of game round (after all players): barbarians move/spawn; city-state turns; world congress if due; climate/sea level/disasters; global era advancement check; victory checks.
