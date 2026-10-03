# 01 Map, Terrain, Features, Resources

## Grid
- Hexagonal tiles ("plots"), pointy-top. Use cube/axial coordinates for math, odd-row offset for storage.
- Map wraps east-west (cylinder) by default; never north-south. Polar rows are snow/tundra/ice.
- Per-plot state: terrain, feature, resource (+ quantity [GS]), improvement (+ pillaged flag), district or wonder, route type, river flags on each of 6 edges (with flow direction), cliff flags on edges, owning player and owning city, worked-by city, appeal (derived), continent id, coastal lowland band (1–3 m) [GS], contamination/fallout turns [GS], and per-player visibility (unrevealed / revealed-fogged / visible).
- Map sizes (dimensions, default majors / city-states): Duel 44×26 (2/3), Tiny 60×38 (4/6), Small 74×46 (6/9), Standard 84×54 (8/12), Large 96×60 (10/15), Huge 106×66 (12/18).

## Terrain
| Terrain | Base yield | Move cost | Defense mod | Notes |
|---|---|---|---|---|
| Grassland | 2F | 1 | 0 | |
| Plains | 1F 1P | 1 | 0 | |
| Desert | 0 | 1 | 0 | Desert floodplains common |
| Tundra | 1F | 1 | 0 | |
| Snow | 0 | 1 | 0 | |
| Hills (any of the above + hills) | base +1P | 2 | +3 | Units on hills see over woods/rainforest/hills |
| Mountains | none, unworkable | impassable | — | Block sight. Give adjacency to Campus, Holy Site; tunnels [GS] late game |
| Coast | 1F 1G | embarked | 0 | Shallow water; early ships restricted to it |
| Lake | 1F 1G | embarked | 0 | Counts as fresh water |
| Ocean | 1F | embarked | 0 | Requires Cartography for embarked land units/most ships |

## Features
| Feature | Yield delta | Move | Defense | Removal | Notes |
|---|---|---|---|---|---|
| Woods | +1P | 2 | +3 | Builder "harvest" (chop) gives one-time P | Blocks sight. Appeal +1 to neighbors. Lumber Mill |
| Rainforest | +1F | 2 | +3 | Chop gives one-time P | Appeal −1 |
| Marsh | +1F | 2 | −2 | Drain | Appeal −1 |
| Floodplains | desert floodplains = 3F total; grass/plains variants [GS] | 1 | −2 | No | [GS] floods |
| Oasis | 3F 1G total | 1 | 0 | No | Fresh water; cannot be improved or have districts |
| Reef | +1F +1P | embarked | +3 | No | Coast only; appeal +1 to neighbors (tunable) |
| Ice | none | impassable | — | No | Melts with climate change [GS] |
| Volcano, Volcanic soil, Geothermal fissure [GS] | soil gives extra F/P/S after eruptions | — | — | — | See climate file |
| Natural wonders | per wonder | usually impassable or 1 | — | No | 1–4 tiles each |

Fresh water source for a city center: adjacent river, lake, or oasis.

### Natural wonders (data rows)
Each has: tile footprint shape, valid terrain, yields to itself and/or adjacent tiles, a special effect, and appeal +2 to neighbors. Discovering one: era score, and boosts Astrology (Eureka: "find a natural wonder"). Examples and effects:
- **Galápagos Islands**: +2 Science to adjacent tiles.
- **Great Barrier Reef**: +3F +2S; Campus adjacency +2.
- **Dead Sea**: +2 Faith +2 Culture; units ending turn adjacent fully heal.
- **Mount Everest**: religious units gain +1 movement if adjacent. Units crossing ignore hill costs (Alpine Training).
- **Mount Kilimanjaro**: +2 Food to adjacent tiles.
- **Cliffs of Dover**: +3 Culture +2 Gold +1F; appeal.
- **Crater Lake**: +4 Faith +1 Science.
- **Uluru**: +2 Faith to adjacent tiles.
- **Yosemite**: +1 Science and Gold to adjacent.
- **Torres del Paine**: doubles terrain yields of adjacent tiles.
- **Bermuda Triangle**: naval units passing gain +1 movement permanently.
- **Fountain of Youth**: units ending turn adjacent heal fully; permanent healing bonus.
- **Mount Vesuvius / Eyjafjallajökull / Kilimanjaro [GS]**: active volcanoes; fertile soil.
Include ~40 total as data; behavior is entirely modifiers.

## Resources
Resource row: id, class (bonus/luxury/strategic/artifact), valid terrains/features, base yield delta, harvesting improvement, reveal tech, improved yield bonus, harvest (removal) yield and required tech, quantity and per-turn accumulation [GS].

### Bonus
Bananas (+1F, plantation, rainforest), Cattle (+1F, pasture), Copper (+2G, mine), Crabs (+2G, fishing boats), Deer (+1P, camp), Fish (+1F, fishing boats), Maize (+2G farm) [GS], Rice (+1F, farm), Sheep (+1F, pasture), Stone (+1P, quarry), Wheat (+1F, farm). Can be harvested (removed) by Builders for a one-time yield once the tech that unlocks the resource's improvement is known: Pottery for wheat/rice/maize, Animal Husbandry for cattle/sheep/deer, Mining for stone/copper, Irrigation for bananas, Celestial Navigation for fish/crabs (tunable).

### Luxury
- Each improved luxury source gives one copy. **One copy = +1 Amenity to each of up to 4 cities** (the 4 that need it most). Duplicate copies are tradeable to other civs via diplomacy.
- Types: Citrus, Cocoa, Coffee, Cotton, Diamonds, Dyes, Furs, Gypsum, Incense, Ivory, Jade, Marble, Mercury, Pearls, Salt, Silk, Silver, Spices, Sugar, Tea, Tobacco, Truffles, Turtles, Whales, Wine, Amber, Olives, Honey. City-state suzerain luxuries and Great Merchant artificial luxuries (Toys, Cosmetics, Jeans, Perfume, Cinnamon, Clove).
- Tile yields: small, e.g. Diamonds +3G, Silk +1C, Whales +1G +1P, Incense +1Fa, Spices +2F, Wine +1F +1G.

### Strategic
| Resource | Revealed by | Typical use |
|---|---|---|
| Horses | Animal Husbandry | Horsemen, Knights, Cavalry, Heavy chariots [GS] |
| Iron | Bronze Working | Swordsman, Knight, iron-based unique units; Railroads [GS] |
| Niter | Military Engineering | Musketman, Bombard, gunpowder-era unique units |
| Coal | Industrialization | Ironclad, Coal Power Plant, Factory power [GS] |
| Oil | Refining | Tanks, Battleships, Aircraft, Oil power plant |
| Aluminum | Radio | Jet fighters/bombers, Modern armor variants, Spaceport projects |
| Uranium | Combined Arms | Nuclear devices, Giant Death Robot, Nuclear power plant |

Two models, pick one (GS recommended):
- **Base model**: each improved source gives copies of the resource; copies are never consumed. A civ that owns at least 1 copy may build any number of units that require that resource. Surplus copies are tradeable.
- **[GS] stockpile model**: each improved source yields ~2/turn early (scales up with techs/policies). Per-resource stockpile cap (~50 base, raised by Encampment buildings, Armory/Military Academy, policies). Unit build cost in resource (e.g., Swordsman 10 Iron, Knight 20 Iron, Musketman 10 Niter, Tank 1 Oil maintenance/turn). Industrial-era-and-later units have per-turn maintenance in resources; a unit without maintenance supply suffers −~20 combat strength and cannot heal. Power plants burn Coal/Oil/Uranium for Power. Upgrading units costs resources too.

## Rivers
- Stored per edge with flow direction; named [GS].
- Crossing a river edge ends the unit's movement unless a road with a bridge crosses there (roads gain bridges after Engineering).
- Attacking across a river: attacker −5 combat strength (Amphibious promotion negates).
- River adjacency: Commercial Hub +2 Gold; city center fresh water housing; Water Mill requires river; Farms gain bonuses via policies/techs.
- [GS] Flooding: floodplain tiles on a river can flood (random disaster). Effects: +1F (and sometimes +1P) permanently to affected floodplains, but pillage improvements and damage units. A Dam district on the river prevents flooding and enables a Hydroelectric Dam (Power).

## Appeal
Plot appeal = sum of adjacent contributions (does not count itself). Data table, examples:
- +2: adjacent Natural Wonder.
- +1: adjacent Woods, Mountain, Coast, Wonder, Holy Site, Theater Square, Entertainment Complex, Water Park, Preserve [GS], Reef.
- −1: adjacent Rainforest, Marsh, Mine, Quarry, Industrial Zone, Encampment, Aerodrome, Spaceport, Airstrip, Oil Well, pillaged/contaminated tiles.
- 0: Floodplains [GS], Neighborhood, City Center, farms and other improvements not listed above.
Bands: Breathtaking ≥4, Charming 2–3, Average −1..1, Uninviting −2..−3, Disgusting ≤−4.
Used by: Neighborhood housing, National Parks (all 4 tiles Charming+), Seaside Resorts (Breathtaking/Charming coast), Ski Resorts [GS], Holy Site / Preserve yields, tourism from improvements.

## Map generation
Map scripts: Continents, Pangaea, Fractal, Island Plates, Archipelago, Small Continents, Inland Sea, Shuffle, Seven Seas, Lakes, Highlands, Tilted Axis, Primordial, Terra (everyone starts on one continent, new world empty), Earth with True Start Locations, Continents and Islands, Splintered Fractal.

Setup options: map type, size, sea level, rainfall, temperature, world age, resource amount, start position balance (Standard/Balanced/Legendary), number of natural wonders, city-states count, barbarians on/off, game speed, difficulty, enabled victory types, turn limit, disasters intensity (0–4) [GS].

Pipeline:
1. Height map via fractal noise or plate simulation; sea level sets land/water.
2. Mountains and hills from world age (old = fewer).
3. Latitude bands + rainfall noise determine terrain.
4. Rivers along edges from highlands to coast; lakes in basins.
5. Features by climate (woods, rainforest near equator, marsh, oasis in desert, floodplains along desert/river, reefs, ice).
6. Natural wonders by placement constraints.
7. Start positions: score tiles for food, production, fresh water, coast, nearby luxuries, distance from others. **Start biases** per civ (e.g., Russia tundra, Egypt floodplain river, England/Norway coast, Inca mountains, Kongo rainforest). Guarantee minimum food/production; balanced starts add resources.
8. Resource distribution: per-region fairness for strategics and luxuries (each start region ~2 luxury types, nearby horses+iron on balanced).
9. City-states placed spaced from majors; tribal villages scattered (~1 per 20–30 land tiles).

## Visibility
- Sight radius: most units 2; scouts 2 with +1 from promotion; city centers 2 (tunable; walls and encampments do not add sight); units on hills see over obstacles.
- Line of sight blocked by mountains, woods, rainforest, hills (unless viewer is elevated or target is elevated).
- Fog shows last-seen terrain, improvements and cities; units hidden.

## Tribal Villages (goody huts)
Entered by a land or embarked unit; consumed. Weighted random reward from categories (data, scaled by era/difficulty): gold (~40–100), faith (~20–100), free Eureka, free Inspiration, free unit (Scout/Builder/Trader), +1 population in nearest city, heal and experience for unit, reveal map area, free relic (R&F changed), era score.

## Barbarians
- Camps spawn on unowned tiles not visible to any major civ, ≥ some distance (~7 tiles) from cities; spawn chance rises with world era and number of revealed tiles.
- Each camp first spawns a **Scout**. If the scout spots a city and returns to its camp, the camp enters raiding mode and spawns 2–3 units per few turns (Warriors/Spearmen/Horsemen if horses nearby, later: Swordsmen, Knights, Musketmen, etc. based on world's tech).
- Naval camps spawn Galleys/Quadriremes and raid coasts.
- Barbarians pillage, capture civilians (Builders, Settlers become Builders when captured), and attack cities (they reduce city HP and blockade, but cannot capture cities).
- Clearing a camp: ~50 gold (scaled), boosts (Bronze Working Eureka: "kill 3 Barbarians"), era score.
- Optional "Barbarian Clans" mode: camps can be bribed, hired, incited against others, and can become city-states.

## Routes
- Roads auto-created when Traders travel routes; Military Engineers build roads manually; road tier upgrades with era (Ancient road 1 MP/tile no bridges, Classical with bridges, Industrial ~0.75, Modern ~0.5 MP/tile).
- Railroads [GS]: built by Military Engineers (Steam Power), cost 1 Iron and 1 Coal per tile (tunable), ~0.1 MP/tile, no tile yields of their own, era score for the first.
- Mountain tunnels [GS]: built by Military Engineers after Chemistry; make the mountain tile passable for units.
