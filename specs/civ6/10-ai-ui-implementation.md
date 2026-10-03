# 10 AI, User Interface, Persistence, Build Roadmap

## AI architecture

### How Civ VI's AI is structured in data
Civ VI's AI is mostly engine code driven by data tables. The data shows the shape a clone can copy (counts are from the ruleset in [`data/`](data/README.md)):

- **Lists of preferences** (data: AiListTypes, AiLists, AiFavoredItems; about 540 lists and 1,300 items). An `AiList` has a *System* that says what it biases and is attached to a leader trait, an agenda trait or a strategy. Systems include Yields, PseudoYields (abstract scores such as "value of a wonder", "value of a settler", "value of standing army"), Technologies, Civics, Buildings, Districts, Units, UnitPromotionClasses, PlotEvaluations, SettlementPreferences, DiplomaticActions, Discussions, Alliances, Resolutions [GS], Strategies, Tactics, Homeland, TriggeredTrees, SavingTypes, AiScoutUses, AiBuildSpecializations. Each item is `(item, favored yes/no, value)`, optionally limited by min/max difficulty. Leaders and agendas are therefore just bundles of weight adjustments on top of the defaults.
- **Default weights** a clone can start from:
  - Yield bias (DefaultYieldBias): Production +25, Gold +20, Science +10, Culture +10, Faith -25.
  - Saving budget split (DefaultSavings): units 4, slush fund 3, great people 1, plot purchase 1.
  - Settling (StandardSettlePlot): fresh water +20, coastal +12, nearest friendly city -10 per tile, new resources +4, iron +5, horses +3, niter +2, luxury and strategic classes +2, foreign continent -2, cultural pressure +1 (or -6 when unfavourable), plus per-yield weights for the inner ring and the total area (Food and Production weighted 2, others 1). City-settle thresholds (DefaultCitySettlement): minimum site value 30 to act, value decays by 3 every 9 turns.
  - Scouting (DefaultScoutUse): land scouts per primary region 100, per secondary region 50, naval world exploration 300.
  - Homeland (non-combat) unit behaviours in priority order (Default Homeland): first-turn settle, great person moves, move to safety, promotion, found national park, religious combat, rock band moves, missionary moves, attack civilians, builders working outside the ring, gather goody huts, explore land, end turn.
  - Tactical behaviours (Default Tactical): attack high/medium/low priority targets, attack districts, attack camps, chase target, coastal raid, pillage district/improvement, heal, move to safety, make military/support/leader formations, air assault/rebase/deploy, wander.
- **Strategies** (data: Strategies, StrategyConditions, Strategy_Priorities). 16 strategies switch extra AiLists on when enough named conditions hold (`NumConditionsNeeded`); a condition can be a *disqualifier* (forces it off) or *exclusive* (enough on its own):
  - Victory strategies, one per victory type: Culture (3 of: good culture, has a Great Writer/Artist/Musician, 3+ great works; exclusive when 75% of needed tourists are taken), Military (3 of: took a capital, 2+ opponents, leads in military strength, leads in score, score gap to the next civ; exclusive with 2 captured capitals), Religious (2 of: good faith city, founded religion, 2+ unconverted cities; disqualified if it cannot found one or its religion is gone), Science (3 of: good tech city, in the Renaissance, 2 science wonders, 33% tech lead, lags military; exclusive after 2 space projects), Diplomatic [GS] (2 of: has diplomatic victory points, leads them, holds 25% of the target; exclusive at 60%).
  - Situational: Early Exploration (fewer cities), Rapid Expansion (3 of: a settle spot available, plus checks on cities under threat and wars with major civs; disqualified at Warlord or lower, forbidden during a Dark Age), Naval (start on an island), Wonder Obsessed (agenda), Dark Age, and one "era changes" strategy per era from Ancient to Modern that swaps yield and pseudo-yield weights as the game progresses.
  - All strategies except Naval and Wonder Obsessed are disqualified for non-major civs.
- **Operations** (data: AiOperationTypes, AiOperationDefs, AiOperationTeams, AiTeams). Military actions are *operations* with a target type, enemy type, a behavior tree to run, a priority, max target distance, a minimum odds of success and whether war is required. The 16 definitions: attack barbarian camp (also a variant to trigger a tech boost), attack enemy city and attack walled city (peacetime planning and wartime variants; walled cities need more units and a siege tree), escort builder to capture (barbarian and civilian), settle new city (escorted settler), city defense, barbarian raid and barbarian city assault, nuclear assault, aid ally [R&F], naval superiority, free-city raid [R&F]. Each operation recruits one or more *teams* with a required strength ratio to start and to continue (for example, a walled city assault wants 2x strength to start and 4x to continue; wartime variants accept lower ratios), plus an optional naval team when the target is coastal.
- **Behavior trees** (data: BehaviorTrees, BehaviorTreeNodes; 30 trees, about 600 nodes). Node types include Sequence, Priority (selector), Concurrent, decorators (Not, Turn Limiter, Contract Manager), conditions (Is At War, Check Plot, Operation Is Ready) and actions (Recruit Units, Make Formation, Operation Move, Operation Attack City/Units, Operation Siege City, Operation Pillage, Build Unit, Add Goal Tech, Purchase Plot, Move Unit). Trees exist for each operation and for triggered jobs: city build triggers, research trigger tech, seek wonder, upgrade units, manage great person, manage spy, manage archaeologist, launch inquisition, build railroad [GS].
- **AI events** (data: AiEvents) wake up triggered trees: gained tech, gained civic, unit captured, wants to settle, archaeologist available, great person gained, spy available, city converted, religion founded, needs railroad.
- **Build specializations** (data: AiBuildSpecializations): a city can be told to build for food, production, gold, science, culture, faith, city defenses, military units or trade units; each picks items by that yield.
- **Tuning constants** (data: GlobalParameters AI_*): trade offered at most every 10 turns, friendship offers every 5, peace offers every 3; base luxury value 1.5; ZOC path discount 0.1; bias toward continuing the current task 80; island coast percentage 20; gold value of aid 150. See [global-parameters.md](data/global-parameters.md#ai).

### Recommended clone architecture
Combining the data structure above with utility scoring:
1. **Grand strategy layer** (re-evaluated every few turns): evaluate the 16 strategy condition sets; active strategies add their AiLists to the leader's base lists. Outputs priorities for research, civics, production, diplomacy, military posture.
2. **Economic planner** (per turn):
   - Research/civic choice: score nodes by unlock value x list weights x boost availability (prefer nodes whose boost is nearly done or achieved; the game's AI only chases boosts from Warlord up).
   - City production: utility for each buildable item = expected yield value / cost, adjusted for needs (housing short -> Granary/Aqueduct; amenities low -> Entertainment; threat -> units/walls; settlers early while good spots exist), filtered by the city's build specialization.
   - District placement: evaluate each candidate plot by adjacency now + predicted future adjacency, preserving good farm/resource tiles; reserve wonder spots.
   - Settler site evaluation: weighted sum as in StandardSettlePlot above, with a minimum value and decay.
   - Builder automation: improve best unimproved tile worked by city, prioritize resources and boost triggers.
   - Citizen management: weighted yield focus with food floor.
   - Policy selection: score cards by value under current state.
3. **Military layer**: threat assessment per city (enemy units within range x strength); operations as above, launched when the strength ratio and minimum odds are met. Unit tactical moves via a tactical map that scores tiles for attack/defense value; focus fire to kill; retreat to heal when badly damaged; avoid ending turn exposed. (These tactical heuristics are recommendations, not documented game values.)
4. **Diplomacy layer**: opinion score per civ from modifiers (agendas, shared enemies, borders/proximity, grievances, deals, religion, government, and the difficulty-scaled random offset). Decide: friendship/alliance proposals, denounce, war declaration (relative military strength, opportunity, agenda, casus belli availability), deal evaluation (value each item in gold-equivalent; demand surplus scaled by opinion), peace when war score is unfavorable or war weariness high.
5. **Religion layer** (if Religious strategy active or agenda says so): faith spend on missionaries/apostles toward cities with low pressure and high population; defend with inquisitors.
6. **City-state layer**: envoy allocation to reach thresholds/suzerainty aligned with strategy.

Difficulty mostly changes bonuses (yields, combat, XP, free boosts, starting units; see the table in `00-overview.md`), plus a few data switches: boost chasing only from Warlord, Rapid Expansion disabled at Warlord or below, the diplomacy random offset, and barbarian spawn distance/throttle. The core AI is the same at every level.

## User interface (screens and overlays)
- **Main map**: 3D/2D hex map with terrain art, units (flags with HP, formation count), cities (banner with name, population, growth turns, production icon/turns, defense strength/HP, religion, loyalty bar [R&F]), district/wonder models, resource icons, yield icons toggle, grid toggle, strategic view (2D map).
- **Top bar**: yields per turn (Science, Culture, Faith, Gold with balance, Tourism), resources (strategic stockpiles [GS], luxuries), era/age, turn number, date, Diplomatic Favor [GS], game menu, reports, civilopedia.
- **Lenses** (overlays; source: https://www.civilopedia.net/en-US/standard-rules/concepts/world_7/): manual lenses Appeal, Continent, Government, Political, Settler (valid tiles, fresh water/housing, loyalty and disaster risk), Tourism, Empire, Loyalty [R&F]; automatic lenses for district/wonder placement (adjacency preview) and Religion (shown when a religious unit is selected). Builder, Archaeologist, Naturalist and similar lenses are popular mods, not base game.
- **Action panel** (bottom right): current research/civic progress, Next Turn button with blocker text (full blocker list in `00-overview.md`).
- **Unit panel**: name, type, HP, MP, CS/RS, promotions, charges, actions (move, attack, ranged, fortify, sleep, alert, heal, skip, pillage, upgrade, promote, build improvement, found city, spread religion, activate GP, form corps/army, delete).
- **Combat preview**: on hover over enemy, show expected damage dealt/received with breakdown of modifiers (terrain, support, river, difficulty bonus, etc.). The difficulty line is shown as its own entry (data: LocalizedText LOC_COMBAT_DIFFICULTY_SCALING).
- **City panel**: yields, population, growth bar, housing and amenities breakdown, religion followers, loyalty breakdown, buildings, great works, citizens management (tile lock), production list (districts with placement mode showing adjacency previews), purchase tab (gold/faith), production queue.
- **Tech tree / Civics tree** screens: zoomable graph, boosts shown with condition text, queue multiple nodes (path auto-queues prerequisites).
- **Government screen**: government choice, policy card drag-and-drop into slots, unlocked governments list, government plaza buildings [R&F].
- **Religion screen**: pantheon/religion choice, beliefs, world religion distribution.
- **Great People screen**: each class's available person, your points/progress, other civs' progress, patronage/pass buttons.
- **Great Works screen**: slots across empire, drag to swap, theming status.
- **Trade route selection**: destinations with yields preview and range.
- **Diplomacy screen**: leader portrait/scene, mood, access level, intel tabs (gossip, agendas, relationship modifiers breakdown, alliances, accessible deals), action list, deal screen (two-column items).
- **City-States screen**: envoys per state, tier bonuses, suzerain, quests, Levy military.
- **Governors screen** [R&F]: titles, promotion trees, assign.
- **Timeline / Historic moments** [R&F], **World Rankings** (progress per victory type), **Reports** (cities yields, resources, deals, city status, gossip), **World Congress** [GS] (resolutions voting), **Climate screen** [GS] (CO2 by civ, phase, sea levels), **Espionage panel**.
- **Notifications** stack for events (data: Notifications; 198 types, each with a severity Low/Mid/High/Very High, an expiry rule and a grouping such as tech boost or civic boost). Click to jump. Blocking notifications mirror the end-turn blockers.
- **Civilopedia**: generated from data definitions.
- **Setup flow**: Single player / Multiplayer / Hot-seat / Scenarios; leader select with abilities; advanced setup (map, rules, victories, speed, difficulty, turn timer).

## Multiplayer
- Turn modes (data: TurnPhases): Simultaneous (all players act at once), Dynamic (simultaneous between players at peace, sequential between players at war; source: https://forums.civfanatics.com/threads/simultaneous-dynamic-turn-mode.603116/), and a two-phase mode (simultaneous strategic segment, then a tactical segment); single player is Sequential. There is no separate "Hybrid" mode in Civ VI. Turn timers: None, Dynamic (time scales with cities and units), Standard. Segment time inputs are in `00-overview.md`.
- Recommended for a clone: lockstep deterministic simulation with command broadcast; desync detection via state hashing each turn.
- Hot seat.

## Persistence
- Save = serialized full game state (map plots, players, cities, units, religions, deals, timers, RNG state, history/timeline, AI memory). Version stamped for compatibility. Autosave every N turns; quick save.
- Replay data: per-turn snapshots of borders, city founding, wars, wonders, to render end-game replay map and graphs (score, science, culture, military strength, etc.).

## Content/data organization (suggested)
```
data/
  terrain.json  features.json  resources.json  improvements.json  routes.json
  districts.json  buildings.json  wonders.json  projects.json
  units.json  promotions.json  unit_classes.json
  techs.json  civics.json  boosts.json
  governments.json  policies.json
  beliefs.json  religions.json
  great_people.json  great_works.json
  civs.json  leaders.json  agendas.json
  city_states.json  governors.json  spy_missions.json
  historic_moments.json  dedications.json  eras.json
  resolutions.json  disasters.json  climate.json
  modifiers.json  requirements.json
  game_speeds.json  difficulty.json  map_sizes.json  scoring.json  map_scripts/
  turn_phases.json  notifications.json
  ai/ lists.json  favored_items.json  strategies.json  operations.json  behavior_trees.json
```
The generated tables in `specs/civ6/data/` and the SQLite rebuild produced by `tools/civ6_extract/build_db.py` are the reference source for filling these files.

## Recommended build order (milestones)
1. **Core map + units (MVP-1)**: hex grid, terrain/features/resources, fog of war, Settler founds city, Warrior/Scout movement with A*, end-turn loop.
2. **Cities (MVP-2)**: population/food/production, tile working, borders by culture, Monument/Granary, unit training, gold and maintenance, housing/amenities simple version.
3. **Research (MVP-3)**: tech tree with the Ancient and Classical techs, civics tree with the Ancient and Classical civics, boosts, Chiefdom + Autocracy/Oligarchy/Classical Republic with policy cards.
4. **Combat (MVP-4)**: combat formula, ZOC, ranged, cities with walls, capture/raze, barbarians, promotions.
5. **Districts (MVP-5)**: Campus, Holy Site, Commercial Hub, Encampment, Theater, Industrial Zone with adjacency; district limit; buildings.
6. **AI (MVP-6)**: basic economic + military AI to provide opponents; diplomacy war/peace only.
7. **Victory (MVP-7)**: Domination and Score; then Science (space projects).
8. Expand content to all eras; Religion; Great People; Trade routes; Tourism/Culture victory.
9. Diplomacy depth: agendas, friendships, alliances, deals; City-states and envoys.
10. R&F systems: Loyalty, Governors, Era score/ages, Emergencies.
11. GS systems: strategic stockpiles, power, climate, disasters, World Congress, Diplomatic victory, grievances.
12. Polish: lenses, civilopedia, notifications, replays, multiplayer.

## Testing strategy
- Deterministic rule unit tests with fixed seeds: combat damage table, growth thresholds, adjacency, district limits, border growth, loyalty math, religious pressure, tourism.
- Data tests: load the rules and assert known values (for example, Standard speed has 500 turns; a Deity AI gets +32% science and 3 Settlers; the Ancient-era boost is 40%).
- Scenario tests: load small handcrafted maps and assert outcomes after N turns.
- AI soak tests: run all-AI games headless to completion; assert no crash, game ends with a victory, performance within budget per turn.
- Golden-file save/load round trips.

## Notes on fidelity
- Values in files 00-10 have been checked against the game's own rules database (Gathering Storm ruleset with Rise and Fall and installed civ packs, all overrides applied). The full generated tables are in [`specs/civ6/data/`](data/README.md), produced by `tools/civ6_extract`. Where a spec file and the generated tables disagree, the tables win.
- Mechanics that are hard-coded in the engine (not in data) are marked `(engine; source: <URL or game file>)` when verified, or `(engine; unverified)` when not. Known open question: the AI combat-strength bonus per difficulty (data formula gives -1 at Prince to +3 at Deity; guides quote +1 at King to +4 at Deity), see `00-overview.md`.
- Brand and asset caution: leader likenesses, names, art, music and text belong to Firaxis/2K. A clone should use original art, names and writing (historical figures are public, but specific wording/art is not). This spec paraphrases all game text; do not copy localization strings into the clone.
