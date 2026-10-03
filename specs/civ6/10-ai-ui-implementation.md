# 10 AI, User Interface, Persistence, Build Roadmap

## AI architecture (recommended)
Civ VI's AI uses behavior trees plus utility scoring with leader-specific weights. A clone can use:
1. **Grand strategy layer** (every ~10 turns): weights for victory types from leader `ai_weights` + current standing (e.g., if far ahead in science, raise science). Outputs priorities for research, civics, production, diplomacy, military posture.
2. **Economic planner** (per turn):
   - Research/civic choice: score nodes by unlock value × strategy weights × boost availability (prefer nodes whose boost is nearly done or achieved).
   - City production: utility for each buildable item = expected yield value / cost, adjusted for needs (housing short → Granary/Aqueduct; amenities low → Entertainment; threat → units/walls; settlers early while good spots exist).
   - District placement: evaluate each candidate plot by adjacency now + predicted future adjacency, preserving good farm/resource tiles; reserve wonder spots.
   - Settler site evaluation: score tiles within N tiles by food, production, fresh water, luxury/strategic, coast, distance to capital, overlap with existing cities, loyalty forecast.
   - Builder automation: improve best unimproved tile worked by city, prioritize resources and boost triggers.
   - Citizen management: weighted yield focus with food floor.
   - Policy selection: score cards by value under current state.
3. **Military layer**: threat assessment per city (enemy units within range × strength); operations (Attack City, Defend City, Clear Camp, Escort Settler, Naval Raid). Unit tactical moves via "tactical analysis map" scoring tiles for attack/defense value; focus fire to kill; retreat to heal at <50% HP; avoid ending turn in ZOC exposure.
4. **Diplomacy layer**: opinion score per civ from modifiers (agendas, shared enemies, borders/proximity, grievances, deals, religion, government). Decide: friendship/alliance proposals, denounce, war declaration (relative military strength, opportunity, agenda, casus belli availability), deal evaluation (value each item in gold-equivalent; demand surplus scaled by opinion and difficulty), peace when war score unfavorable or war weariness high.
5. **Religion layer** (if religious leader): faith spend on missionaries/apostles toward cities with low pressure and high population; defend with inquisitors.
6. **City-state layer**: envoy allocation to reach thresholds/suzerainty aligned with strategy.

Difficulty only changes bonuses and a few heuristics, not the core AI.

## User interface (screens and overlays)
- **Main map**: 3D/2D hex map with terrain art, units (flags with HP, formation count), cities (banner with name, population, growth turns, production icon/turns, defense strength/HP, religion, loyalty bar [R&F]), district/wonder models, resource icons, yield icons toggle, grid toggle, strategic view (2D map).
- **Top bar**: yields per turn (Science, Culture, Faith, Gold with balance, Tourism), resources (strategic stockpiles [GS], luxuries), era/age, turn number, date, Diplomatic Favor [GS], game menu, reports, civilopedia.
- **Lenses** (overlays): Settler (water/housing availability, invalid tiles), Appeal, Religion (followers/pressure), Loyalty [R&F], Governor, Continent, Tourism, Government, Builder (suggested improvements), Archaeologist, Naturalist, Power [GS], Empire.
- **Action panel** (bottom right): current research/civic progress, Next Turn button with blocker text.
- **Unit panel**: name, type, HP, MP, CS/RS, promotions, charges, actions (move, attack, ranged, fortify, sleep, alert, heal, skip, pillage, upgrade, promote, build improvement, found city, spread religion, activate GP, form corps/army, delete).
- **Combat preview**: on hover over enemy, show expected damage dealt/received with breakdown of modifiers ("+3 Defensive terrain, +2 Support, −5 River...").
- **City panel**: yields, population, growth bar, housing and amenities breakdown, religion followers, loyalty breakdown, buildings, great works, citizens management (tile lock), production list (districts with placement mode showing adjacency previews), purchase tab (gold/faith), production queue.
- **Tech tree / Civics tree** screens: zoomable graph, boosts shown with condition text, queue multiple nodes (path auto-queues prerequisites).
- **Government screen**: government choice, policy card drag-and-drop into slots, unlocked governments list, government plaza buildings.
- **Religion screen**: pantheon/religion choice, beliefs, world religion distribution.
- **Great People screen**: each class's available person, your points/progress, other civs' progress, patronage/pass buttons.
- **Great Works screen**: slots across empire, drag to swap, theming status.
- **Trade route selection**: destinations with yields preview and range.
- **Diplomacy screen**: leader portrait/scene, mood, access level, intel tabs (gossip, agendas, relationship modifiers breakdown, alliances, accessible deals), action list, deal screen (two-column items).
- **City-States screen**: envoys per state, tier bonuses, suzerain, quests, Levy military.
- **Governors screen** [R&F]: titles, promotion trees, assign.
- **Timeline / Historic moments** [R&F], **World Rankings** (progress per victory type), **Reports** (cities yields, resources, deals, city status, gossip), **World Congress** [GS] (resolutions voting), **Climate screen** [GS] (CO2 by civ, phase, sea levels), **Espionage panel**.
- **Notifications** stack (left side) for events: unit needs orders, city grew, production done, enemy sighted, disaster, deal offer, etc. Click to jump.
- **Civilopedia**: generated from data definitions.
- **Setup flow**: Single player / Multiplayer / Hot-seat / Scenarios; leader select with abilities; advanced setup (map, rules, victories).

## Multiplayer
- Turn modes: Simultaneous (all players act at once, combat locks), Hybrid (simultaneous in peace, sequential while at war), Dynamic. Turn timer options.
- Lockstep deterministic simulation with command broadcast; desync detection via state hashing each turn.
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
  game_speeds.json  difficulty.json  map_sizes.json  map_scripts/
```

## Recommended build order (milestones)
1. **Core map + units (MVP-1)**: hex grid, terrain/features/resources, fog of war, Settler founds city, Warrior/Scout movement with A*, end-turn loop.
2. **Cities (MVP-2)**: population/food/production, tile working, borders by culture, Monument/Granary, unit training, gold and maintenance, housing/amenities simple version.
3. **Research (MVP-3)**: tech tree with ~20 ancient/classical techs, civics tree with ~15 civics, boosts, Chiefdom + Autocracy/Oligarchy/Classical Republic with policy cards.
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
- Scenario tests: load small handcrafted maps and assert outcomes after N turns.
- AI soak tests: run all-AI games headless to completion; assert no crash, game ends with a victory, performance within budget per turn.
- Golden-file save/load round trips.

## Notes on fidelity
- The text in files 01–09 describes mechanics faithfully at the level of systems; specific numeric values and per-item effects are best-effort recollections of the shipped game and should be treated as defaults. Anything marked `~` or "data" is explicitly a tuning parameter.
- Brand and asset caution: leader likenesses, names, art, music and text belong to Firaxis/2K. A clone should use original art, names and writing (historical figures are public, but specific wording/art is not).
