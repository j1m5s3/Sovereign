# Sovereign: world scale, generator and model sets

Status: brainstorm, not spec (2026-10-04). Companion to [leader-character-brainstorm.md](leader-character-brainstorm.md). Counts below come from our Civ VI reference tables in `specs/civ6/data/`.

## Decisions

- **[decided, James 2026-10-04]** Everything inside a hex is generated and not player-editable: street layout, house and landmark placement are fixed by the generator. Players build only at hex level, as in Civ (districts, improvements, wonders, buildings). Resources stay per hex; map generation stays random per game as in Civ VI.

## The idea in one paragraph

The hex map stays the game. The walkable 3D world is a **rendering of the hex map**, built automatically by a generator from each hex's game data, out of reusable model sets. Nobody hand-builds the world; artists build the pieces and rules, and the generator assembles every hex. Zoomed out you see the Civ map; zoomed in you are on the street of that same place, with no loading screens.

## Scale

- **About 1 km per hex** (compressed, like Civ itself compresses geography). A city center or district fills its hex at roughly real size; countryside between cities is compressed.
- A Standard map (Civ VI: 84 × 54 hexes, about 4,500) is then roughly 84 × 54 km of walkable world.
- **Turn rule:** each turn the leader can roam freely within the hexes its movement points reach; entering a new hex spends points like any unit move. Time is paused within a turn.

## The generator

### Inputs: what each hex already knows

Everything the generator needs is already game state from the Civ VI spec:

| Input | From | What it drives |
|---|---|---|
| Terrain + hills/mountain | 01, `terrain-features-resources.md` | Ground height and surface material |
| Feature (woods, rainforest, marsh, floodplains, reef, oasis, ice) | 01 | Vegetation and water scatter |
| River edges, cliffs, coast | 01 | Carved river channels along hex edges, cliff faces, shorelines |
| Resource | 01 | Visible resource props (sheep, iron outcrop, wheat) |
| Improvement (59 types) | `improvements.md` | Field layouts, mines, pastures, quarries |
| Road tier | 05 / 07 | Road surface and width by era |
| District (36 types) + buildings in it (85 types) | 03, `districts.md`, `buildings.md` | Street layout, landmark buildings, filler blocks |
| Wonder (53 world wonders) | `wonders.md` | Hand-made hero model filling the hex |
| Owner civ + current era | 09 | Which model set is used |
| Population, housing, amenities, loyalty | 02 | Crowd size, housing density, mood visuals |
| Pillaged, disasters, sea level [GS] | 05, 09 | Damaged, flooded or burned variants |

### Seed: same hex, same result

Each hex gets a seed from the map seed plus its coordinates. The same inputs and seed always produce the same result. Two consequences:

- The world never has to be saved as geometry. Only game state is saved, and the world is rebuilt from it.
- **Multiplayer:** every player's machine generates the same world from the same game state, so nothing but normal game data is sent over the network.

### Stages

1. **Ground.** Build the height of each hex from its terrain type (flat, hills, mountain), blended with its six neighbours so there are no seams. Carve rivers along the hex edges where Civ puts them; add coasts and cliffs.
2. **Surface and nature.** Paint the ground material (grassland, plains, desert, tundra, snow). Scatter trees, rocks and plants by density rules for the feature (dense rainforest, open woods), placed so they never overlap.
3. **Land use.** Split the hex into lots. Improvements claim lots using layout templates: a farm becomes field strips following the slope, a mine gets an entrance into the hillside, a pasture gets fences and animals. Roads connect the hex center to each neighbour that also has a road, so roads line up across hexes.
4. **Settlement (city centers and districts).**
   - **Streets:** a main street from the center to each edge that leads to a neighbouring district, road or river crossing; a plaza at the center; side streets splitting the rest into blocks.
   - **Landmarks:** every building the district actually has (Library, Market, Barracks…) gets a landmark model placed on the plaza or main street. Build one in game and it appears here.
   - **Filler:** houses and shops fill the remaining blocks. How many follows population and housing, so a size-3 town and a size-20 city on the same hex look very different.
   - **Walls:** Ancient, Medieval and Renaissance walls become a wall ring around the city center hex.
   - **Wonders:** a wonder replaces generation for its hex with its hand-made model, with the generator only blending the edges.
5. **Life and mood.** Crowds sized by population. Mood from amenities and loyalty: banners and markets when happy, boarded-up shops and graffiti when unhappy, visible guards and patrols when ruled by Fear. Lighting by era: torches, then gas lamps, then electric.
6. **Distance versions.** Each hex is produced in three versions: the Civ-style map tile, a simplified mid-distance model, and full street detail. Only hexes near the camera or the leader load in full detail.

### Changes during play

When game state changes, only that hex rebuilds (sometimes its neighbours too, for blending). Nice side effect: **construction progress can be shown**. A building being produced shows as scaffolding, growing with the city's production progress, and completes when Civ completes it.

## The model sets

A model set ("kit") is a library of modular pieces that snap together on a shared grid: wall sections, roofs, doors, windows, columns, stairs, market stalls, fences. Buildings are assembled from pieces by rules, which gives variety without modelling every building by hand.

### What varies

- **Era:** nine eras, but neighbours can share. Suggest five era bands: Ancient/Classical, Medieval, Renaissance/Industrial, Modern/Atomic, Information/Future.
- **Civ style:** Civ VI groups civs into a handful of regional architecture styles (European, Mediterranean, Middle Eastern, Asian, African, American; exact grouping not in our spec, unverified). Six styles is a reasonable target.
- **Kit count:** 6 styles × 5 era bands = **30 building kits**, each roughly 150–300 pieces. One kit can still produce many buildings.

### What is shared

- **Nature kit:** terrain materials, vegetation, rocks, water. Doesn't depend on civ or era; has climate and disaster variants.
- **Infrastructure kit:** roads per era, bridges, railroads, canals, dams.
- **Characters:** citizens per style and era band; the leader's gear reuses the unit models of the same tech (fits the loadout design).

### What is hand-made (hero models)

These are too distinctive to assemble from pieces:

- **53 world wonders.**
- **Unique districts, buildings and improvements** of each civ (part of the 36 districts, 85 buildings and 59 improvements in our tables).
- **Palace per style**, plus a few landmark buildings per kit that anchor the look.
- **3+ natural wonders.**

### How the rules pick pieces

A building recipe says, for example, "Library: 2-storey footprint 20 × 15 m, columned entrance, reading hall, era band determines window type". The generator takes the civ's kit for its era band and fills the recipe with that kit's pieces. A Greek Classical library and a Japanese Classical library come from the same recipe and different kits.

## Production order

1. **Prove the pipeline with one slice:** one style, the Ancient/Classical band, plus the nature kit, a city center and one district. If it looks good there, the method holds.
2. Add the remaining districts and improvements for that slice.
3. Add styles and era bands one at a time; each is mostly artist work, not code.
4. Wonders and unique buildings go in steadily throughout.

## Engine fit (suggestion)

Any modern engine can do this, but Unreal Engine 5 ships most of the parts: World Partition (streaming a large world in pieces), Nanite (very detailed geometry with automatic simplification by distance) and its procedural content framework for the scatter and assembly stages. Unity and Godot can do it too, with more of it built in-house.

## Risks

- **Sameness:** generated cities can look repetitive. Mitigate with enough recipe variants, landmark buildings and the per-hex seed.
- **Rebuild hitches:** a large city changing many hexes at once. Rebuild over several frames and show construction states.
- **Art consistency across 30 kits:** needs a strict style guide and shared grid from day one.
