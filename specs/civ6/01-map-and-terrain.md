# 01 Map, Terrain, Features, Resources

Full per-row data: [data/terrain-features-resources.md](data/terrain-features-resources.md), [data/improvements.md](data/improvements.md), [data/game-setup.md](data/game-setup.md), [data/barbarians-goody-huts.md](data/barbarians-goody-huts.md). This file describes the rules; the data files are the source for numbers not repeated here.

## Grid
- Hexagonal tiles ("plots"), pointy-top. Use cube/axial coordinates for math, odd-row offset for storage.
- Map wraps east-west (cylinder) by default; never north-south. Polar rows are snow/tundra/ice.
- Per-plot state: terrain, feature, resource (+ quantity [GS]), improvement (+ pillaged flag), district or wonder, route type, river flags on each of 6 edges (with flow direction), cliff flags on edges, owning player and owning city, worked-by city, appeal (derived), continent id, coastal lowland band (1–3 m) [GS], contamination/fallout turns [GS], and per-player visibility (unrevealed / revealed-fogged / visible).
- Map sizes (data: Maps): grid and default major civs are Duel 44×26 (2), Tiny 60×38 (4), Small 74×46 (6), Standard 84×54 (8), Large 96×60 (10), Huge 106×66 (12). Natural wonders placed per size: 2/3/4/5/6/7. Default city-state counts come from the setup (front-end) config, not the rules database: Duel 3, Tiny 6, Small 9, Standard 12, Large 15, Huge 18 (setup config; unverified). See [data/game-setup.md](data/game-setup.md).

## Terrain
Values from Terrains / Terrain_YieldChanges. "Move" is the movement cost to enter; water tiles cost 1 for embarked/naval units.

| Terrain | Base yield | Move cost | Defense mod | Appeal (to neighbours) | Notes |
|---|---|---|---|---|---|
| Grassland | 2F | 1 | 0 | 0 | |
| Plains | 1F 1P | 1 | 0 | 0 | |
| Desert | 0 | 1 | 0 | 0 | Floodplains and oases appear here |
| Tundra | 1F | 1 | 0 | 0 | |
| Snow | 0 | 1 | 0 | 0 | |
| Hills (any of the above + hills) | base +1P (Plains Hills 1F 2P) | 2 | +3 | 0 | Sight +1; hills block line of sight for lower units |
| Mountains | none, unworkable | impassable | — | +1 | Sight +2 and block sight. Adjacency for Campus and Holy Site; tunnels [GS] late game |
| Coast | 1F 1G | 1 (embarked/naval) | 0 | +1 | Shallow water; early ships restricted to it |
| Lake | 1F 1G | 1 (embarked/naval) | 0 | +1 | Stored as the Coast terrain plus a lake flag; counts as fresh water |
| Ocean | 1F | 1 (embarked/naval) | 0 | 0 | Embarked land units and most early ships need Cartography to enter |

## Features
Values from Features / Feature_YieldChanges / Feature_Removes. "Move" is the movement cost after the feature's +1 change.

| Feature | Yield delta | Move | Defense | Appeal | Removal | Notes |
|---|---|---|---|---|---|---|
| Woods | +1P | 2 | +3 | +1 | Builder harvest (needs Mining): one-time 20P | Blocks sight. Lumber Mill |
| Rainforest | +1F | 2 | +3 | −1 | Harvest (needs Bronze Working): one-time 10P + 10F | Blocks sight |
| Marsh | +1F | 2 | −2 | −1 | Drain (needs Irrigation): one-time 20F | |
| Floodplains | Desert floodplains +2F (2F total); Grassland/Plains floodplains [GS] add nothing on top of the base terrain | 1 | −2 | −1 | No | [GS] floods; districts may be built on them |
| Oasis | +3F +1G (3F 1G total) | 1 | 0 | +1 | No | Fresh water; no city can be founded on it |
| Reef | +1F +1P | 1 (water) | +3 | 0 | No | Coast only |
| Ice | none | impassable | — | 0 | No | Melts with climate change [GS] |
| Volcano, Volcanic soil, Geothermal fissure (+1 Science) [GS] | soil gains extra F/P/S after eruptions | — | — | 0 | — | See climate file |
| Burning / burnt woods and rainforest [GS] | none | 2 | +3 | −1 | — | Wildfire states |
| Natural wonders | per wonder | most impassable | — | +2 (Cliffs of Dover and Uluru +4) | No | 1–4 tiles each |

Harvest yields above are base values; they scale with game progress (the higher of techs or civics researched as a share of the total), from the base value at the start up to about 10× at 100% progress, and are multiplied by game speed and by production-for-buildings style bonuses (engine; source: https://forums.civfanatics.com/threads/when-to-chop-harvest.615519/).

Fresh water source for a city center: adjacent river, lake, or oasis (also a few natural wonders flagged AddsFreshWater: Crater Lake, Ik-Kil, Pamukkale, Fountain of Youth).

### Natural wonders (data rows)
Each has: tile footprint (1–4 tiles), valid terrain, yields to itself and/or to adjacent tiles, an optional special effect (a GameModifier or adjacency rule), and an appeal contribution to neighbours (+2, or +4 for Cliffs of Dover and Uluru). Discovering one awards era score, grants a little unit XP (data: EXPERIENCE_REVEAL_NATURAL_WONDER = 10) and triggers the Astrology Eureka. The installed ruleset has 34; full table in [data/terrain-features-resources.md](data/terrain-features-resources.md#natural-wonders). Representative effects:
- **Galápagos Islands** (impassable): adjacent tiles +2 Science.
- **Great Barrier Reef** (workable): +3F +2S; adjacent Campus +2 Science adjacency.
- **Dead Sea** (workable): +2 Faith +2 Culture; land units ending a turn adjacent heal fully.
- **Mount Everest** (impassable): adjacent tiles +1 Faith; land units that end adjacent gain a permanent ability to ignore hills movement cost.
- **Mount Kilimanjaro** (impassable, volcano in [GS]): adjacent tiles +2 Food.
- **Cliffs of Dover** (workable): +3 Culture +3 Gold +2 Food; appeal +4 to neighbours.
- **Crater Lake** (workable, fresh water): +5 Faith +1 Science.
- **Uluru** (impassable): adjacent tiles +2 Faith +2 Culture; appeal +4.
- **Yosemite** (impassable): adjacent tiles +1 Gold +1 Science +1 Food.
- **Torres del Paine** (impassable): doubles the terrain yields of adjacent tiles.
- **Bermuda Triangle** (ocean, passable): adjacent tiles +5 Science; naval units entering it get +1 movement permanently and are teleported to another tile of the wonder (teleport destination rule: engine; unverified).
- **Fountain of Youth** (passable): +4 Science +4 Faith; land units entering it gain a permanent +10 HP healing per turn.
- **Pamukkale**: fresh water, adjacent districts gain extra yields, +1 Amenity per natural wonder to the owning city.
- **Giant's Causeway**: adjacent land units +5 combat strength. **Lysefjord**: adjacent naval units gain a promotion's worth of XP. **Ik-Kil**: +50% Production for wonders built adjacent. **Païtiti**: +4 Gold on international trade routes from the owning city.
- **Mount Vesuvius / Eyjafjallajökull / Kilimanjaro [GS]**: active volcanoes; eruptions create volcanic soil.
Behaviour is entirely data (Feature_* tables, Feature_AdjacentYields, GameModifiers), so implement natural wonders as data rows plus modifiers.

## Resources
Resource row: id, class (bonus/luxury/strategic/artifact), valid terrains/features, base yield delta, harvesting improvement, reveal tech, improved yield bonus, harvest (removal) yield and required tech, quantity and per-turn accumulation [GS]. Full tables: [data/terrain-features-resources.md](data/terrain-features-resources.md#bonus-resources).

### Bonus
Bananas (+1F, plantation, rainforest), Cattle (+1F, pasture), Copper (+2G, mine), Crabs (+2G, fishing boats), Deer (+1P, camp), Fish (+1F, fishing boats), Maize (+2G, farm) [GS], Rice (+1F, farm), Sheep (+1F, pasture), Stone (+1P, quarry), Wheat (+1F, farm). Builders can harvest (remove) them for a one-time yield once the listed tech is known (data: Resource_Harvests): Pottery for Wheat/Rice (20F) and Maize (40G); Animal Husbandry for Cattle/Sheep (20F) and Deer (20P); Mining for Copper (40G); Masonry for Stone (20P); Irrigation for Bananas (20F); Celestial Navigation for Fish (20F) and Crabs (40G). These base amounts scale like woods harvests (see above).

### Luxury
- Each improved luxury source gives one copy. **Each luxury type you hold gives +1 Amenity to each of up to 4 cities** (the cities that need it most); extra copies of the same type add no amenities but can be traded to other civs. City-state luxuries Cinnamon and Cloves, and Perfume, cover 6 cities (data: Resources.Happiness).
- Types: Amber, Citrus, Cocoa, Coffee, Cotton, Diamonds, Dyes, Furs, Gypsum, Honey, Incense, Ivory, Jade, Marble, Mercury, Olives, Pearls, Salt, Silk, Silver, Spices, Sugar, Tea, Tobacco, Truffles, Turtles, Whales, Wine. Non-map luxuries granted by effects (city-state suzerainty, Great Merchants): Cinnamon, Cloves, Toys, Cosmetics, Jeans, Perfume.
- Tile yields are small, e.g. Diamonds +3G, Silk +1C, Whales +1G +1P, Incense +1Fa, Spices +2F, Wine +1F +1G.

### Strategic
| Resource | Revealed by | Typical use |
|---|---|---|
| Horses | Animal Husbandry | Horseman, Cavalry, Courser and other light/heavy cavalry uniques |
| Iron | Bronze Working | Swordsman, Man-at-Arms, Knight, Cuirassier, iron-based uniques; Railroads [GS] |
| Niter | Military Engineering | Musketman, Line Infantry, Bombard, Frigate, gunpowder uniques |
| Coal | Industrialization | Ironclad, Battleship (and their maintenance), Coal Power Plant [GS], Railroads [GS] |
| Oil | Refining | Infantry, Artillery, Tank, Destroyer, Submarine, Biplane and later land/naval units; Oil Power Plant |
| Aluminum | Radio | Fighters, Bombers, Helicopter and jets; Orbital Laser project [GS] |
| Uranium | Combined Arms | Nuclear / thermonuclear device projects (10 / 20), Giant Death Robot, Nuclear Power Plant |

Two models, pick one (GS recommended):
- **Base model**: each improved source gives copies of the resource; copies are never consumed. A civ that owns at least 1 copy may build any number of units that require that resource. Surplus copies are tradeable.
- **[GS] stockpile model** (data: Resource_Consumption, Units_XP2):
  - Each improved source adds 2/turn (Horses, Iron, Niter, Aluminum) or 3/turn (Coal, Oil, Uranium); unimproved sources add 0. Policies, governors and some civ abilities add more per source.
  - Stockpile cap per resource is 50, raised by +10 for each Barracks/Stable, Armory and Military Academy the player owns (plus a few civ uniques).
  - Unit build cost in resource: Swordsman, Horseman, Knight, Musketman and most other pre-industrial resource units 20; many uniques 5–10. Industrial-and-later units cost 1 and have per-turn maintenance (e.g. Tank 1 Oil/turn, Ironclad 1 Coal/turn, Giant Death Robot 3 Uranium/turn).
  - A unit whose maintenance cannot be paid loses up to 20 combat strength (data: UNIT_MAX_STR_REDUCTION_INSUFFICIENT_RESOURCES) and cannot heal.
  - Power plants burn Coal/Oil/Uranium for Power (4/4/16 power per unit, see the climate file). Upgrading units costs resources too.

## Rivers
- Stored per edge with flow direction; named [GS].
- Crossing a river edge costs extra movement (in practice 3 MP into flat land, which ends most early units' moves) unless a road with a bridge crosses there (engine; source: https://civilization.fandom.com/wiki/River_(Civ6)). Roads support bridges from the Classical-era road tier onwards (data: Routes.SupportsBridges).
- Attacking across a river: attacker −5 combat strength (data: COMBAT_RIVER_DEFENSE; the Amphibious promotion negates it). Attacking from an embarked position is −10 (data: COMBAT_AMPHIBIOUS_ATTACK_PENALTY).
- River adjacency: Commercial Hub +2 Gold; city center fresh water housing; Water Mill requires river; Farms gain bonuses via policies/techs.
- [GS] Flooding: floodplain tiles on a river can flood (random disaster). Effects: +1F (and sometimes +1P) permanently to affected floodplains, but pillage improvements and damage units. A Dam district on the river prevents flooding and enables a Hydroelectric Dam (Power).

## Appeal
A tile's appeal is the sum of contributions from the tiles adjacent to it (its own terrain does not count) (engine; source: https://civilization.fandom.com/wiki/Appeal_(Civ6)). Per-item contributions are data (the Appeal column of Terrains, Features, Improvements, Districts):
- +4: adjacent Cliffs of Dover or Uluru. +2: any other adjacent natural wonder.
- +1 each: adjacent Mountain, Coast/Lake, Woods, Oasis, wonder, Holy Site, Theater Square, Entertainment Complex, Water Park, Dam, Canal, Preserve [GS]; some improvements (City Park +2, Château, Pairidaeza, Rock-Hewn Church +1).
- +1 once if the tile is next to a river or lake (engine; source above).
- −1 each: adjacent Rainforest, Marsh, Floodplains, burning/burnt woods or rainforest [GS], Mine, Quarry, Oil Well, Offshore Oil Rig, Airstrip, Industrial Zone, Encampment, Aerodrome, Spaceport (and their uniques), Barbarian camps, pillaged tiles.
- 0: Reef, Neighborhood, City Center, Farms and other improvements not listed.
Bands: Breathtaking ≥4, Charming 2–3, Average −1..1, Uninviting −3..−2, Disgusting ≤−4 (data: AppealHousingChanges).
Used by: Neighborhood housing (4 base, −2..+2 by band), Preserve [GS] yields, National Parks (all 4 tiles Charming or better), Seaside Resorts (minimum appeal 4), Ski Resorts [GS], Holy Site adjacency beliefs, tourism from appeal-based improvements.

## Map generation
Map scripts: Continents, Pangaea, Fractal, Island Plates, Archipelago, Small Continents, Inland Sea, Shuffle, Seven Seas, Lakes, Highlands, Tilted Axis, Primordial, Terra (everyone starts on one continent, new world empty), Earth with True Start Locations, Continents and Islands, Splintered Fractal.

Setup options: map type, size, sea level, rainfall, temperature, world age, resource amount, start position balance (Standard/Balanced/Legendary), number of natural wonders, city-states count, barbarians on/off, game speed, difficulty, enabled victory types, turn limit, disasters intensity (0–4) [GS].

Pipeline:
1. Height map via fractal noise or plate simulation; sea level sets land/water.
2. Mountains and hills from world age (old = fewer).
3. Latitude bands + rainfall noise determine terrain.
4. Rivers along edges from highlands to coast; lakes in basins.
5. Features by climate (woods, rainforest near equator, marsh, oasis in desert, floodplains along rivers, reefs, ice).
6. Natural wonders by placement constraints (count per map size above).
7. Start positions: score tiles for food, production, fresh water, coast, nearby luxuries, distance from others (data: START_DISTANCE_MAJOR_CIVILIZATION = 12 between majors, 6 between a major and a city-state, 5 between city-states). **Start biases** per civ (data: StartBias* tables; e.g. Russia tundra, Egypt floodplain river, England/Norway coast, Inca mountains, Kongo rainforest). Guarantee minimum food/production; balanced starts add resources.
8. Resource distribution: per-region fairness for strategics and luxuries (each start region gets its own luxury types; balanced starts place horses and iron nearby).
9. City-states placed spaced from majors; tribal villages scattered by the map script (density is a script parameter; unverified).

## Visibility
- Sight radius: most units 2 (data: Units.BaseSightRange); Settler 3; Scout promotions add +1. Standing on hills adds +1 sight (data: Terrains.SightModifier). City center sight radius (engine; unverified).
- Line of sight is blocked by mountains, woods, rainforest and hills unless the viewer is elevated above the blocker.
- Fog shows last-seen terrain, improvements and cities; units hidden.

## Tribal Villages (goody huts)
Entered by a land or embarked unit; consumed (the entering unit gains 5 XP, data: EXPERIENCE_ACTIVATE_GOODY_HUT). Reward selection is two-stage weighted random (data: GoodyHuts, GoodyHutSubTypes): first one of 7 equal-weight categories (Culture, Gold, Faith, Military, Science, Survivors, Diplomacy), then a reward inside it; some rewards only appear after a minimum turn. Main rewards:
- Gold: 40 / 75 / 120 (weights 55/30/15; medium from turn 20, large from turn 40). Faith: 20 / 60 / 100 (weights 55/30/15).
- Science: 1 Eureka (55), 2 Eurekas (30, turn 30+), a free tech (15, turn 50+). Culture: 1 Inspiration (55), 2 Inspirations (30), a Relic (15).
- Survivors: +1 population in the nearest city (40), a Builder (35), a Trader (25, turn 15+).
- Military: a recon unit (35), heal the unit (25), +20 XP (20), +20 of your most advanced strategic resource (20, scaled by game speed).
- Diplomacy: an envoy (40), +20 Diplomatic Favor (45, turn 30+), a Governor title (15, turn 30+).
Full table: [data/barbarians-goody-huts.md](data/barbarians-goody-huts.md).

## Barbarians
- Camps spawn on unowned tiles not visible to any major civ, at least 4 tiles from any city (+1 per difficulty level below Prince) and at least 7 from another camp, at most 3 camps per major civ (data: BARBARIAN_CAMP_*). Coastal camps are naval tribes; camps within 3 tiles of Horses are cavalry tribes; otherwise melee (data: BarbarianTribes).
- Each camp first spawns a **Scout**. If the scout sees a city and returns to its camp, the camp turns aggressive and starts releasing raid or attack forces. Force composition depends on difficulty and tribe (e.g. Warlord–Emperor standard raid: 2 melee + 1 ranged; standard attack adds siege and a battering ram); see [data/barbarians-goody-huts.md](data/barbarians-goody-huts.md). Unit types follow technology known by the majors (data: BARBARIAN_TECH_PERCENT = 50). A boldness counter (data: BARBARIAN_BOLDNESS_*) rises each turn and with kills, falls when the camp loses units, and decides when it attacks.
- Naval camps spawn Galleys/Quadriremes and raid coasts.
- Barbarians pillage, capture civilians (a captured Settler becomes a Builder), and attack cities (they reduce city HP and blockade, but cannot capture cities) (engine; unverified).
- Clearing a camp: a gold reward (engine; unverified amount), the Bronze Working Eureka after killing 3 barbarian units (data: Boosts), era score.
- Optional "Barbarian Clans" mode: camps can be bribed, hired, incited against others, and can become city-states.

## Routes
Data: Routes, Routes_XP2, Route_ResourceCosts.
- Roads are created automatically along Traders' routes; Military Engineers can build them manually (1 charge). The road tier follows the owner's era: Ancient road 1 MP/tile without bridges, Classical road 1 MP/tile with bridges, Industrial road 0.75 MP/tile, Modern road 0.5 MP/tile.
- Railroads [GS]: built only by Military Engineers after Steam Power; cost 1 Iron and 1 Coal per tile (no charge), 0.25 MP/tile, no tile yields of their own.
- Mountain tunnels [GS]: built by Military Engineers after Chemistry; make the mountain tile passable for units.
