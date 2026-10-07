# 03 Districts, Buildings, Wonders, Builders and Improvements

Reference tables (generated from the game rules database): [districts](data/districts.md), [buildings](data/buildings.md), [wonders](data/wonders.md), [improvements](data/improvements.md), [projects](data/projects.md), [global parameters](data/global-parameters.md). Narrative values below were checked against those tables; when they disagree, the data tables win.

## Districts: general rules
- A district occupies one tile inside the city's workable radius (≤3) on land the city owns (Harbor, Water Park, Canal-type exceptions use water/coast tiles as their placement rules say). Placing a district clears Woods, Rainforest and Marsh and any improvement on the tile. It cannot go on a Mountain or a natural wonder, nor on a visible luxury or strategic resource; a bonus resource is removed when built over, and a strategic resource that is still hidden does not block placement (it is granted once revealed) (engine; source: https://steamcommunity.com/app/289070/discussions/0/154642447914707429/).
- Placement flags come from data: one of each type per city (`OnePerCity`), Encampment and Preserve may not be adjacent to the City Center, Government Plaza and Diplomatic Quarter are limited to one per civilization (`MaxPerPlayer = 1`), Entertainment Complex and Water Park are mutually exclusive in a city (`MutuallyExclusiveDistricts`), Aerodrome/Spaceport/Canal need flat land, Dam needs Floodplains (one per river). See the "Placement/flags" column of [districts](data/districts.md).
- Once placed, the tile is reserved and production accumulates on it; the player may switch production away and later resume without losing the stored progress (engine; unverified).
- **District cap** (population limit): a city may hold `1 + floor((pop − 1) / DISTRICT_POPULATION_REQUIRED_PER)` districts that require population, with `DISTRICT_POPULATION_REQUIRED_PER = 3` (pop 1: 1, pop 4: 2, pop 7: 3, ...) (engine formula; source: https://civilization.fandom.com/wiki/District_(Civ6)). Which districts count is data (`Districts.RequiresPopulation`): every specialty district counts, including Government Plaza, Diplomatic Quarter, Aerodrome and Preserve. City Center, Aqueduct (and Bath), Neighborhood (and Mbanza), Canal, Dam and Spaceport do not count.
- **District cost.** Base costs (Standard speed): 54 for most districts, 27 for unique replacements (half), 36 Aqueduct (18 Bath), 30 Government Plaza and Diplomatic Quarter, 81 Canal and Dam, 1800 Spaceport (fixed). Cost growth with game progress: `cost = base × (1 + 9 × max(T, C))`, where T and C are the fractions of the technology and civics trees completed (only finished nodes count, partial progress and boosts do not), so costs rise up to ×10 over the game (engine; source: https://civilization.fandom.com/wiki/District_(Civ6)). In data each district names a progression model (`Districts.CostProgressionModel` / `CostProgressionParam1`):
  - `COST_PROGRESSION_NUM_UNDER_AVG_PLUS_TECH` (all specialty districts): the tree-progress growth above plus a **discount of Param1 percent** (40; 25 for Government Plaza and Diplomatic Quarter) for district types the player has built less than their own average. The comparison uses the player's own history, not other civs: with A = number of specialty district types the player has unlocked and B = number of such districts the player has completed, a type is discounted when B ≥ A and B / A is greater than the number of districts of that type the player already has placed. B only updates when a tech or civic completes after the district finishes (engine; source: https://forums.civfanatics.com/resources/civ-vi-district-discounts.27783/).
  - `COST_PROGRESSION_GAME_PROGRESS` (Param1 = 1000: Aqueduct, Bath, Neighborhood, Mbanza, Preserve, Canal, Dam): tree-progress growth only, never discounted. Community measurements show the same roughly ×10 growth as above; the exact way the engine applies Param1 = 1000 is unverified (a literal "+1000% at full progress" reading would give ×11) (engine; unverified).
  - `NO_COST_PROGRESSION`: City Center, Spaceport, Wonder pseudo-district.
- **Adjacency bonuses** come only from `District_Adjacencies` → `Adjacency_YieldChanges` rows. Each row gives a fixed yield either per adjacent qualifying tile or per N tiles (`TilesRequired = 2` rows, the "per 2" bonuses). The generic "per 2 districts" row counts any adjacent district including the City Center, Aqueduct, Canal, Dam and Neighborhood, but not wonders (source: https://gamerant.com/civilization-vi-adjacency-bonuses-civ-6-adjacencies-district-bonuses-explained/). Rounding: each "per 2" row is rounded down on its own (engine; unverified). Adjacency is recomputed whenever a neighbouring tile changes. Yields go to the district tile and its city.
- Policies that add +100% to a district's adjacency yield (data: `PolicyModifiers`): Natural Philosophy (Campus), Scripture (Holy Site), Aesthetics (Theater Square), Town Charters (Commercial Hub), Naval Infrastructure (Harbor), Craftsmen (Industrial Zone); later replacements Five-Year Plan (Campus + Industrial Zone), Economic Union (Commercial Hub + Harbor), Sports Media (Theater Square).
- **Defense**: City Center (200 HP in data) and Encampment (100 HP) are the only combat districts. The Encampment gains outer defenses and a ranged strike (range 2) once the city has Walls (engine; source: https://forums.civfanatics.com/resources/city-combat.27737/). Encampment, Government Plaza and Diplomatic Quarter carry `CityStrengthModifier = 2`.
- **Citizen slots**: each building with `CitizenSlots` adds specialist slots to its district; specialists earn the district's `District_CitizenYieldChanges` yields (e.g. Campus +2 Science, Commercial Hub +4 Gold, Encampment +2 Gold +1 Production) plus any per-building specialist yields.
- **Pillage**: a pillaged district or building stops working until repaired (repair costs production; `PILLAGE_BUILDING_REPAIR_PERCENT = 25`). Plunder yields are per district (`Districts.PlunderType/PlunderAmount`): Campus and Industrial Zone 25 Science, Holy Site 25 Faith, Theater Square/Government Plaza/Diplomatic Quarter 25 Culture, Commercial Hub/Harbor/Aerodrome/Neighborhood/Aqueduct/Canal/Preserve 50 Gold, Entertainment Complex/Water Park/Dam heal 50 HP; Encampment gives nothing.
- **Great Person points**: each specialty district gives +1 point per turn of its class (`District_GreatPersonPoints`): Campus → Scientist, Holy Site → Prophet, Theater Square → Writer, Artist and Musician (+1 each), Commercial Hub → Merchant, Harbor → Admiral, Encampment → General, Industrial Zone → Engineer. Some uniques give +2 (Lavra, Royal Navy Dockyard). Buildings add their own points (usually +1, more for e.g. Art Museum +2 Artist +1 Writer); see [buildings](data/buildings.md).
- Appeal from districts: Holy Site, Theater Square, Entertainment Complex, Water Park, Preserve, Canal and Dam +1; Encampment, Industrial Zone, Aerodrome and Spaceport −1 (data: `Districts.Appeal`).

## District table

Adjacency lists every `Adjacency_YieldChanges` row for the generic district; "Gov Plaza +1" means +1 when adjacent to the Government Plaza [R&F].

| District | Unlock | Adjacency (yield to district) | Buildings (in order) |
|---|---|---|---|
| City Center | — | — | Palace; Monument; Granary; Water Mill (river); Ancient Walls → Medieval Walls → Renaissance Walls; Sewer; Flood Barrier [GS] |
| Campus (Science) | Writing | +1 per Mountain; +1 per 2 Rainforest; +1 per 2 districts; +2 per Reef [GS]; +2 per Geothermal Fissure [GS]; +2 Great Barrier Reef; +2 Pamukkale; Gov Plaza +1 | Library (+2 Science) → University (+4 Science, +1 Housing) → Research Lab (Chemistry; +3 Science, +5 more when powered, needs 3 Power [GS]) |
| Holy Site (Faith) | Astrology | +2 per natural wonder; +1 per Mountain; +1 per 2 Woods; +1 per 2 districts; +1 Pamukkale; Gov Plaza +1 | Shrine (+2 Faith) → Temple (Theology; +4 Faith, 1 Relic slot) → one worship building from the founder's belief (Cathedral, Gurdwara, Meeting House, Mosque, Pagoda, Synagogue, Wat, Stupa, Dar-e Mehr; bought with Faith) |
| Theater Square (Culture) | Drama and Poetry | +2 per wonder; +2 per Entertainment Complex or Water Park (and their uniques); +2 Pamukkale; +1 per 2 districts; Gov Plaza +1 | Amphitheater (+2 Culture, 2 Writing slots) → Art Museum (3 Art slots) OR Archaeological Museum (3 Artifact slots) (Humanism, +2 Culture each) → Broadcast Center (Radio; +2 Culture, +4 more when powered, 1 Music slot, 3 Power) |
| Commercial Hub (Gold) | Currency | +2 if on a river; +2 per Harbor (or Royal Navy Dockyard/Cothon); +2 Pamukkale; +1 per 2 districts; Gov Plaza +1 | Market (+2 Gold, +1 Trade Route capacity) → Bank (Banking; +5 Gold) → Stock Exchange (Economics; +4 Gold, +7 more when powered, 3 Power) |
| Harbor (Gold) | Celestial Navigation | +2 City Center; +1 per sea resource; +1 per 2 districts; Gov Plaza +1. On Coast/Lake adjacent to land | Lighthouse (+1 Housing, +1 Food on the city's Coast/Lake tiles, +1 Trade Route capacity if the city has no Market, +25% XP for naval units) → Shipyard (Mass Production; +1 Food, +1 Production on unimproved Coast/Lake tiles, +25% XP for naval units) → Seaport (Electricity; +2 Gold +2 Food, +1 Housing, +2 Gold on Coast/Lake tiles, +25% XP for naval units) |
| Industrial Zone (Production) | Apprenticeship | +1 per Quarry; +1 per strategic resource; +1 per 2 Mines; +1 per 2 Lumber Mills; +2 per Aqueduct/Bath/Canal/Dam; +1 per 2 districts; Gov Plaza +1 | Workshop (+3 Production) → Factory (Industrialization; +3 Production, +3 more when powered, regional 6 tiles, 2 Power) → one power plant: Coal (Industrialization), Oil (Electricity) or Nuclear (Nuclear Fission); regional 6 tiles, supplies Power [GS] |
| Encampment | Bronze Working | none; not adjacent to City Center | Barracks (Bronze Working) OR Stable (Horseback Riding) (+1 Production, +1 Housing) → Armory (Military Engineering; +3 Production) → Military Academy (Military Science; +4 Production, +25% production toward corps/armies). Each: +25% XP for units trained in the city (Barracks: melee, ranged and anti-cavalry; Stable: light and heavy cavalry and siege; Armory and Military Academy: both groups), +10 strategic stockpile cap [GS] |
| Entertainment Complex | Games and Recreation | none (district gives +1 Amenity) | Arena (+1 Culture, +2 Amenity) → Zoo (Natural History; +1 Amenity regional 6 tiles) → Stadium (Professional Sports; +1 Amenity regional 6 tiles, +2 more when powered, 2 Power) |
| Water Park [GS] | Natural History | none (district gives +1 Amenity); Coast/Lake adjacent to land; excludes Entertainment Complex in the same city | Ferris Wheel (+3 Culture, +2 Amenity) → Aquarium (+1 Amenity regional 9 tiles) → Aquatics Center (Professional Sports; +1 Amenity regional 9 tiles, +2 more when powered, 2 Power) |
| Aqueduct | Engineering | must be adjacent to the City Center and to a River, Lake, Oasis or Mountain (engine; source: https://www.civilopedia.net/en-US/standard-rules/districts/district_aqueduct/) | none. Housing: brings a city without fresh water up to 6 (`CITY_POPULATION_AQUEDUCT_MIN`), or +2 if it already has fresh water (`CITY_POPULATION_AQUEDUCT_BOOST`); +2 Industrial Zone adjacency; prevents drought [GS]; +1 Amenity next to a Geothermal Fissure [GS] |
| Neighborhood | Urbanization | none; housing by appeal (4 base, +2 Breathtaking … −2 Disgusting) | Food Market (Replaceable Parts) OR Shopping Mall (Capitalism); 1 Power each [GS] |
| Aerodrome | Flight | none; flat land | Hangar (Flight; +25% XP for fighters and bombers) → Airport (Advanced Flight; +50% XP for fighters and bombers); +1 air slot each (district has 2) |
| Spaceport | Rocketry | none; flat land | Space race projects |
| Government Plaza [R&F] | State Workforce | gives +1 to adjacent specialty districts | one building per tier, each tier requires a government of that tier (engine; unverified): Tier 1 Audience Chamber / Ancestral Hall / Warlord's Throne; Tier 2 Foreign Ministry / Intelligence Agency / Grand Master's Chapel (Queen's Bibliotheque replaces them for Kristina only); Tier 3 War Department / National History Museum / Royal Society |
| Diplomatic Quarter [GS] | Mathematics | none | Consulate (Mathematics) → Chancery (Diplomatic Service); +1 Envoy if built next to the City Center; foreign spies 2 levels lower against it and the districts beside it; +1 Favor per turn per delegation or embassy other civs keep with you (source: https://www.civilopedia.net/gathering-storm/districts/district_diplomatic_quarter) |
| Canal [GS] | Steam Power | flat land; must link two bodies of water or water to the City Center | lets naval units and trade pass; +2 Industrial Zone adjacency |
| Dam [GS] | Buttress | on Floodplains along a river, one per river | Hydroelectric Dam (Electricity; +6 Power); prevents floods and drought; +3 Housing, +1 Amenity |
| Preserve [GS] | Mysticism | not adjacent to City Center; housing by appeal | Grove (Mysticism) → Sanctuary (Conservation): unimproved adjacent tiles gain yields by appeal (+1 at Charming, +2 at Breathtaking; data: `Adjacent_AppealYieldChanges`) |

Unique districts replace a generic one, cost half, and change adjacency or effects (all rows in [districts](data/districts.md)): Acropolis (Greece), Bath (Rome), Hansa (Germany), Lavra (Russia), Mbanza (Kongo), Royal Navy Dockyard (England), Street Carnival and Copacabana (Brazil), Ikanda (Zulu) [R&F], Seowon (Korea) [R&F], Cothon (Phoenicia), Hippodrome (Byzantium), Observatory (Maya), Oppidum (Gaul), Thành (Vietnam), Suguba (Mali).

### Example adjacency computation
```
def campus_adjacency(plot):
    s = 0
    for n in neighbors(plot):
        if n.terrain == MOUNTAIN: s += 1
        if n.feature in (REEF, GEOTHERMAL_FISSURE): s += 2
        if n.natural_wonder in (GREAT_BARRIER_REEF, PAMUKKALE): s += 2
        if n.district == GOVERNMENT_PLAZA: s += 1
        rainforest += n.feature == RAINFOREST
        districts += n.has_district   # City Center counts; wonders do not
    s += rainforest // 2 + districts // 2   # each "per 2" row floored separately
    return s * (1 + policy_bonus_pct / 100) # e.g. Natural Philosophy: +100%
```

## Buildings
- Built inside a district (or the City Center). Each has: production cost, gold maintenance, prerequisites (tech/civic and previous building; where several earlier buildings are listed, any one of them will do, e.g. the Armory needs a Barracks or a Stable), mutual exclusions (a city never holds both: Barracks/Stable, Art Museum/Archaeological Museum, power plants, Food Market/Shopping Mall, the Government Plaza tiers; data: Exclusive with), yields, housing, amenities, Great Work slots, citizen slots, GPP, regional range, and in [GS] a Power requirement and extra yields when powered. Values: [buildings](data/buildings.md).
- Regional buildings (Factory, power plants, Zoo, Stadium: 6 tiles; Aquarium, Aquatics Center: 9 tiles) apply their effect to every city of the owner within range; a city benefits only once from each regional building type (engine; unverified).
- City Center buildings: Palace (+2 Science, +5 Gold, +2 Production, +1 Culture, +1 Housing, +2 Amenity, 1 Great Work slot), Monument (+1 Culture, +1 Loyalty [R&F], +1 more Culture at full loyalty), Granary (Pottery; +1 Food, +2 Housing), Water Mill (Wheel; river only; +1 Food +1 Production, +1 Food on Rice/Wheat/Maize), Ancient/Medieval/Renaissance Walls (Masonry/Castles/Siege Tactics; +100 outer HP each), Sewer (Sanitation; +2 Housing), Flood Barrier [GS] (Computers; blocks coastal flooding; cost scales with flooded tiles).
- Power [GS]: power-consuming buildings need 1–3 Power each (Factory 2, Stadium 2, Aquatics Center 2, Research Lab 3, Stock Exchange 3, Broadcast Center 3, Food Market 1, Shopping Mall 1, Airport 1). Unpowered buildings still give their base yields; the "extra when powered" yields (`Building_YieldChangesBonusWithPower`) and powered amenity bonuses apply only when the city's Power demand is met (all-or-nothing per city: engine; unverified). Supply: power plants burn a strategic resource for Power (Coal 4, Oil 4, Uranium 16 Power per unit; data: `Resource_Consumption.PowerProvided`) and serve every city within 6 tiles; Hydroelectric Dam, Solar Farm, Wind Farm, Offshore Wind Farm and Geothermal Plant give local CO2-free Power. Burning fossil fuels emits CO2 (see [climate](data/climate-disasters.md)).

## Wonders
- Each World Wonder can be built once in the world (`MaxWorldInstances = 1`). It occupies its own tile (the Wonder pseudo-district, +1 Appeal to neighbours) and has a tech or civic prerequisite plus placement rules from data: terrain/feature, river, coast, adjacency to a district (sometimes with a specific building in the city, `BuildingPrereqs`, e.g. Great Library needs a Library), adjacency to an improvement or resource. Not on resources or existing districts.
- When a rival completes a wonder the city is building, the wonder leaves the build list and about half of the production invested is kept as overflow for the city's next item [R&F] (engine; source: https://forums.civfanatics.com/threads/beaten-to-a-wonder-production-reibursment.632300/).
- Wonders provide yields, Great Work slots, GPP, free units, era score and Tourism (base +2 per wonder, `TOURISM_BASE_FROM_WONDER`).

Wonder list (unlock, era from data; full effects, costs and placement in [wonders](data/wonders.md)). 53 wonders in the Gathering Storm ruleset with the installed packs.

| Wonder | Unlock (era) | Placement | Effect summary |
|---|---|---|---|
| Stonehenge | Astrology (Ancient) | flat, next to Stone | +2 Faith; free Great Prophet (an Apostle if no Prophet is left) |
| Hanging Gardens | Irrigation (Ancient) | next to river | +2 Housing; +15% growth in all cities |
| Great Bath [GS] | Pottery (Ancient) | Floodplains | +3 Housing, +1 Amenity; this river's floodplains stop taking flood damage; +1 Faith on flooded tiles per mitigated flood |
| Pyramids | Masonry (Ancient) | Desert/desert floodplains | +2 Culture; free Builder; all Builders +1 charge |
| Etemenanki [DLC] | Writing (Ancient) | Floodplains or Marsh | +2 Science; Marsh +2 Science +1 Production; this city's floodplains +1 Science +1 Production |
| Temple of Artemis [R&F] | Archery (Ancient) | next to a Camp | +4 Food, +3 Housing; each Camp, Pasture and Plantation within 4 tiles gives +1 Amenity |
| Oracle | Mysticism (Ancient) | hills | +1 Culture +1 Faith; this city's districts give +2 GPP of their type; Great People patronage with Faith 25% cheaper |
| Great Lighthouse | Celestial Navigation (Classical) | coast, next to Harbor with Lighthouse | +3 Gold, +1 Admiral point; naval units +1 movement; embarked units +1 movement |
| Colossus | Shipbuilding (Classical) | coast, next to Harbor | +3 Gold, +1 Admiral point; +1 Trade Route capacity; free Trader |
| Petra | Mathematics (Classical) | Desert/floodplains | Desert tiles in the city (not floodplains) +2 Food +2 Gold +1 Production |
| Colosseum | Games and Recreation (Classical) | flat, next to Entertainment Complex with Arena | +2 Culture; +2 Amenity and +2 Loyalty [R&F] to own cities within 6 tiles |
| Great Library | Recorded History (Classical) | flat, next to Campus with Library | +2 Science, +1 Scientist and +1 Writer point, 2 Writing slots; all Ancient and Classical Eurekas; a random Eureka whenever another civ recruits a Great Scientist |
| Mahabodhi Temple | Theology (Classical) | Woods, next to Holy Site with Temple | +4 Faith; 2 Apostles; +2 Diplomatic Victory points [GS] |
| Terracotta Army | Construction (Classical) | flat Grassland/Plains next to Encampment with Barracks or Stable | +1 General point; all current land units gain a promotion level; Archaeologists may enter foreign territory |
| Machu Picchu [GS] | Engineering (Classical) | Mountain | +4 Gold; Commercial Hub, Theater Square and Industrial Zone +1 per adjacent Mountain in all cities |
| Statue of Zeus [DLC] | Military Training (Classical) | flat, next to Encampment with Barracks | +3 Gold; free Spearmen, Archers and a Battering Ram; +50% production toward anti-cavalry units |
| Apadana [DLC] | Political Philosophy (Classical) | next to the capital | 2 Great Work slots; +2 Envoys whenever a wonder is completed in this city |
| Mausoleum at Halicarnassus [DLC] | Defensive Tactics (Classical) | next to Harbor | Great Engineers +1 charge; this city's coast tiles +1 Science +1 Faith +1 Culture |
| Jebel Barkal [DLC] | Iron Working (Classical) | Desert Hills | +4 Faith to own cities within 6 tiles; +6 Iron per turn |
| Hagia Sophia | Buttress (Medieval) | flat, next to Holy Site | +4 Faith; Missionaries and Apostles +1 spread charge |
| Alhambra | Castles (Medieval) | hills, next to Encampment | +2 Amenity, +2 General points; +1 Military policy slot |
| Chichen Itza | Guilds (Medieval) | Rainforest | Rainforest tiles in the city +2 Culture +1 Production |
| Mont St. Michel | Divine Right (Medieval) | Floodplains or Marsh | +2 Faith, 2 Relic slots; all Apostles gain the Martyr promotion |
| Kotoku-in [R&F] | Divine Right (Medieval) | next to Holy Site with Temple | +20% Faith in the city; 4 Warrior Monks |
| Kilwa Kisiwani [R&F] | Machinery (Medieval) | flat, next to coast | +3 Envoys; +15% to the city-state type yield of each city-state you are suzerain of in this city, and a further +15% in all cities when suzerain of 2+ of a type |
| Meenakshi Temple [GS] | Civil Service (Medieval) | next to Holy Site | +3 Faith; 2 Gurus; religious unit bonuses next to Gurus; Gurus 30% cheaper |
| University of Sankore [GS] | Education (Medieval) | Desert, next to Campus with University | +3 Science +1 Faith, +2 Scientist points; trade route bonuses for routes from other civs |
| Huey Teocalli [DLC] | Military Tactics (Medieval) | Lake tile next to land | +1 Amenity per adjacent Lake tile; all your Lake tiles +1 Food +1 Production |
| Angkor Wat [DLC] | Medieval Faires (Medieval) | next to Aqueduct | +2 Faith; +1 Population and +1 Housing in all cities |
| Venetian Arsenal | Mass Production (Renaissance) | coast, next to Industrial Zone | +2 Engineer points; every naval melee, ranged or carrier unit trained gives a second copy |
| Great Zimbabwe | Banking (Renaissance) | next to Commercial Hub with Market and to Cattle | +5 Gold, +2 Merchant points; +1 Trade Route capacity; routes from this city +2 Gold per bonus resource in its territory |
| Forbidden City | Printing (Renaissance) | flat, next to City Center | +5 Culture; +1 Wildcard policy slot |
| Casa de Contratación [R&F] | Cartography (Renaissance) | next to Government Plaza | +3 Merchant points; +3 Governor titles; +15% Faith, Gold and Production in governed cities on other continents |
| St. Basil's Cathedral [R&F] | Reformed Church (Renaissance) | next to City Center | 3 Relic slots; city's Tundra tiles +1 Food +1 Production +1 Culture; doubled religious Tourism from the city |
| Taj Mahal [R&F] | Humanism (Renaissance) | next to river | +1 era score for each historic moment worth 2 or more |
| Torre de Belém [DLC] | Mercantilism (Renaissance) | coast, next to Harbor | +5 Gold, +1 Admiral point; international routes +2 Gold per luxury at destination; each city on another continent gets the cheapest building of its districts |
| Potala Palace | Astronomy (Renaissance) | hills next to a Mountain | +2 Culture +3 Faith; +1 Diplomatic policy slot; +1 Diplomatic Victory point [GS] |
| Ruhr Valley | Industrialization (Industrial) | river, next to Industrial Zone with Factory | +20% Production; city's Mines and Quarries +1 Production |
| Bolshoi Theatre | Opera and Ballet (Industrial) | flat, next to Theater Square | +2 Writer +2 Musician points; 1 Writing + 1 Music slot; 2 random civics |
| Oxford University | Scientific Theory (Industrial) | flat Grassland/Plains next to Campus with University | +3 Scientist points, 2 Writing slots; +20% Science; 2 random techs |
| Big Ben | Economics (Industrial) | river, next to Commercial Hub with Bank | +6 Gold, +3 Merchant points; +1 Economic policy slot; treasury +50% on completion [GS] |
| Hermitage | Natural History (Industrial) | next to river | +3 Artist points; 4 Art slots |
| Országház [DLC] | Sanitation (Industrial) | next to river | +4 Culture; +100% Diplomatic Favor from suzerainties |
| Panama Canal [GS] | Steam Power (Industrial) | as a canal (multi-tile) | +10 Gold; acts as a canal |
| Statue of Liberty [R&F] | Civil Engineering (Industrial) | coast, next to Harbor | own cities within 6 tiles never lose loyalty; +4 Diplomatic Victory points [GS] |
| Eiffel Tower | Steel (Modern) | flat, next to City Center | +2 Appeal on all tiles of your cities |
| Broadway | Mass Media (Modern) | flat, next to Theater Square | +3 Writer +3 Musician points; 1 Writing + 2 Music slots; +20% Culture; a random Atomic-era Inspiration |
| Cristo Redentor | Mass Media (Modern) | hills | +4 Culture; Seaside Resort Tourism ×2; religious Tourism is never reduced by later-era rules |
| Golden Gate Bridge [GS] | Combustion (Modern) | coast tile spanning two opposite land tiles | land bridge with a modern road; +3 Amenity, +4 Appeal in the city; +100% Tourism from improvements and National Parks in the city |
| Amundsen-Scott Research Station [R&F] | Cold War (Atomic) | Snow, next to Campus with Research Lab | +5 Scientist points; +20% Science and +10% Production in all cities, doubled in cities with 5+ Snow tiles |
| Estádio do Maracanã | Professional Sports (Atomic) | flat, next to Entertainment Complex with Stadium | +6 Culture; +2 Amenity in all your cities |
| Biosphère [GS] | Synthetic Materials (Atomic) | river, next to Neighborhood | +1 Appeal next to Rainforest and Marsh; renewable Power output +200% and +100% Tourism from it in all cities |
| Sydney Opera House | Cultural Heritage (Atomic) | coast, next to Harbor | +8 Culture, +5 Musician points; 3 Music slots |

[DLC] = civ/leader pack content that is part of the installed ruleset. All wonder data must stay tunable.

## Builders and improvements
- **Builder**: civilian with 3 charges (`Units.BuildCharges`). Extra charges: Pyramids +1 (all builders), Serfdom and Public Works +2 for builders trained. Ilkum and Public Works +30% Builder production. Each improvement, resource harvest or feature removal costs 1 charge; repairing a pillaged improvement or district costs none (engine; unverified). The builder is removed at 0 charges.
- Building an improvement completes instantly and ends the builder's movement for the turn (engine; unverified).
- Feature removal requires: Woods → Mining, Rainforest → Bronze Working, Marsh → Irrigation (data: `Features.RemoveTech`).
- **Military Engineer**: 2 charges. Builds roads, railroads (Steam Power; Military Engineer only), Forts, Airstrips, Missile Silos and Mountain Tunnels [GS], and can spend a charge to add 20% of the production cost to an Aqueduct, Bath, Canal or Dam in progress (data: `District_BuildChargeProductions`).

| Improvement | Unlock | Valid on | Base yield | Notes (data) |
|---|---|---|---|---|
| Farm | — | Grassland, Plains, Floodplains, Volcanic Soil; Grassland/Plains Hills with Civil Engineering | +1 Food | +1 Food per 2 adjacent Farms (Feudalism), replaced by +1 per adjacent Farm (Replaceable Parts); +0.5 Housing |
| Mine | Mining | Hills, Volcanic Soil, mineable resources | +1 Production | +1 each with Apprenticeship, Industrialization, Smart Materials; Appeal −1 |
| Quarry | Mining | Stone, Marble, Gypsum | +1 Production | +1 each with Gunpowder, Rocketry, Predictive Systems; Appeal −1 |
| Pasture | Animal Husbandry | Cattle, Horses, Sheep | +1 Production | +1 Food Stirrups, +1 Production Replaceable Parts, +1 Food Robotics; +0.5 Housing |
| Camp | Animal Husbandry | Deer, Furs, Honey, Ivory, Truffles | +2 Gold | +1 Food +1 Production Mercantilism, +2 Gold Synthetic Materials; +0.5 Housing |
| Plantation | Irrigation | Bananas, Citrus, Cocoa, Coffee, Cotton, Dyes, Incense, Olives, Silk, Spices, Sugar, Tea, Tobacco, Wine | +2 Gold | +1 Food Feudalism, +1 Food Scientific Theory, +2 Gold Globalization; +0.5 Housing |
| Fishing Boats | Sailing | Amber, Crabs, Fish, Pearls, Turtles, Whales | +1 Food | +2 Gold Cartography, +1 Production Colonialism, +1 Food Plastics; +0.5 Housing |
| Lumber Mill | Construction | Woods, Rainforest | +2 Production | +1 Steel, +1 Cybernetics |
| Oil Well / Offshore Oil Rig | Refining / Plastics | Oil (land / sea) | +2 Production | +1 Predictive Systems; Appeal −1 |
| Fort | Siege Tactics | any flat or hill land | — | +4 defense, grants fortification; Military Engineer |
| Airstrip | Flight | flat land | — | 3 air slots; Military Engineer |
| Missile Silo | Rocketry | flat land | — | 1 weapon slot; Military Engineer |
| Seaside Resort | Radio | flat coastal land, Appeal ≥ 4 | Gold = Appeal | Tourism = Appeal |
| Ski Resort [GS] | Professional Sports | Mountain | — | Tourism = Appeal; +1 Amenity |
| Solar Farm [GS] | Satellites | flat Desert/Grassland/Plains/Tundra | +1 Gold +1 Production | +2 Power |
| Wind Farm [GS] | Composites | Hills | +2 Gold +1 Production | +2 Power |
| Offshore Wind Farm [GS] | Predictive Systems | Coast/Lake | +2 Production | +2 Power |
| Geothermal Plant [GS] | Synthetic Materials | Geothermal Fissure | +2 Production +1 Science | +4 Power |
| Mountain Tunnel [GS] | Chemistry | Mountain | — | movement through mountains; Military Engineer |
| Seastead [GS] | Seasteads | Coast/Ocean | +2 Food | +4 Housing; adjacency from Fishing Boats and Reef |

Unique improvements (unlocks, placement and adjacency in [improvements](data/improvements.md)): Ziggurat (Sumeria), Sphinx (Egypt), Stepwell (India), Château (France), Great Wall (China), Kurgan (Scythia), Mission (Spain), Hacienda (Gran Colombia), Pairidaeza (Persia), Golf Course (Scotland) [R&F], Polder (Netherlands) [R&F], Outback Station (Australia), Kampung (Indonesia), Terrace Farm (Inca) [GS], Mekewap (Cree) [R&F], Nubian Pyramid (Nubia), Chemamull (Mapuche) [R&F], Open-Air Museum (Sweden) [GS], Ice Hockey Rink (Canada) [GS], Rock-Hewn Church (Ethiopia), Pā (Māori) [GS], Feitoria (Portugal), Qhapaq Ñan (Pachacuti). City-state suzerain improvements (Monastery, Mahavihara, Colossal Head, Moai, Nazca Line, etc.) are listed there too.

## Projects
Repeatable or one-time production items run in a city or district; costs and conversions in [projects](data/projects.md).
- District projects (base cost 25, `GAME_PROGRESS` 1500): convert part of the city's production into a yield and give GPP while active: Campus Research Grants, Holy Site Prayers, Theater Square Festival, Commercial Hub Investment, Harbor Shipping, Industrial Zone Logistics, Encampment Training; Bread and Circuses (Entertainment Complex or Water Park) grants loyalty. Brazil's Street Carnival/Copacabana run Carnival instead.
- Space race (Spaceport): Launch Earth Satellite (Rocketry; reveals the map), Launch Moon Landing (Satellites), Launch Mars Colony (Nanotechnology), Launch Exoplanet Expedition (Smart Materials) [GS], then Lagrange Laser Station and Terrestrial Laser Station (Offworld Mission) [GS] to speed the expedition.
- [GS]: Carbon Recapture (Global Warming Mitigation), Convert to Coal/Oil/Nuclear Power, Decommission Coal/Oil/Nuclear Power Plant, Recommission Nuclear Reactor, Repair Outer Defenses, Send Aid, Train Athletes, Train Astronauts. Flood Barrier is a building, not a project.
- Nuclear: Manhattan Project (Nuclear Fission; once per player, required before Build Nuclear Device, 10 Uranium each), Operation Ivy (Nuclear Fusion; required before Build Thermonuclear Device, 20 Uranium each).
