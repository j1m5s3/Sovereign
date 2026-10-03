# 03 Districts, Buildings, Wonders, Builders and Improvements

## Districts: general rules
- A district occupies one tile inside the city's workable radius (≤3) on owned land. It removes most features (woods/rainforest/marsh) and the tile's improvement; it cannot be placed on mountains, natural wonders, or strategic/luxury resources (bonus resources are removed; luxury and strategic block placement).
- Once placed, the tile is reserved; production accumulates. Districts can be placed then switched away from without losing progress.
- **District cap**: specialty districts allowed = `1 + floor((pop − 1) / 3)` (pop 1: 1, 4: 2, 7: 3, ...). Non-specialty districts not counted: Aqueduct, Neighborhood, Spaceport, Canal, Dam, City Center. Government Plaza and Diplomatic Quarter count as specialty and are limited to one per civ.
- Each specialty district type: max one per city.
- **District cost**: `base × (1 + 9 × progress)` where progress = max(fraction of tech tree completed, fraction of civics tree completed), with a **−40% discount** if the player has fewer of that district type than the average across all players (tunable). Unique districts cost half.
- **Adjacency bonuses**: recomputed when nearby tiles change. Rules come in three magnitudes: Major (+2), Standard (+1), Minor (+1 per 2 sources, i.e., +0.5 each, floor). Yields come only to the district (and its city). Policies double specific adjacency bonuses: Natural Philosophy (Campus), Scripture (Holy Site), Aesthetics (Theater Square), Town Charters (Commercial Hub), Naval Infrastructure (Harbor), Craftsmen (Industrial Zone).
- Only the City Center and Encampment have hit points: Encampment has 100 HP and a ranged attack once the city has walls. Other districts can be pillaged.
- Specialist slots: equal to number of buildings in the district.
- Pillage: enemy pillages district → loses function until repaired; pillage yields gold/science/faith/culture by district type.
- **Great Person points**: each specialty district gives +1 GPP of its class per turn (Campus→Scientist, Holy Site→Prophet, Theater→Writer/Artist/Musician, Commercial Hub→Merchant, Harbor→Admiral, Encampment→General, Industrial Zone→Engineer); each building in them adds +1.

## District table

| District | Unlock | Adjacency (yield to district) | Buildings (in order) |
|---|---|---|---|
| City Center | — | — | Monument, Granary, Water Mill (river), Ancient Walls → Medieval Walls → Renaissance Walls, Sewer |
| Campus (S) | Writing | +2 Reef/Geothermal [GS], +1 per Mountain, +1 per 2 Rainforest, +1 per 2 districts; Great Barrier Reef +2 | Library (+2S, 2 Writing slots) → University (+4S, +1 Housing) → Research Lab (+5S, requires Chemistry; needs Power [GS]) |
| Holy Site (Fa) | Astrology | +2 Natural Wonder, +1 per Mountain, +1 per 2 Woods, +1 per 2 districts | Shrine (+2Fa, can buy Missionaries) → Temple (+4Fa, Relic slot, buy Apostles/Inquisitors/Gurus) → Worship building (from Worship belief: Cathedral, Gurdwara, Meeting House, Mosque, Pagoda, Synagogue, Wat, Stupa, Dar-e Mehr) |
| Theater Square (C) | Drama and Poetry | +1 per Wonder, +2 per Entertainment Complex/Water Park [GS], +1 per 2 districts | Amphitheater (+2C, 2 Writing slots) → Art Museum (3 Art slots) OR Archaeological Museum (3 Artifact slots) → Broadcast Center (+4C, 1 Music slot) |
| Commercial Hub (G) | Currency | +2 River, +2 Harbor, +1 per 2 districts | Market (+3G, +1 Trade Route capacity) → Bank (+5G) → Stock Exchange (+7G; Power [GS]) |
| Harbor (G/F) | Celestial Navigation | +2 City Center, +1 per Coastal resource, +1 per 2 districts. Must be on Coast/Lake adjacent to land | Lighthouse (+1F, +1 Housing, +1G on coast tiles (tunable)) → Shipyard (+P = district adjacency; naval units +XP) → Seaport (+2G, +1F per coast tile, +2 Housing) |
| Industrial Zone (P) | Apprenticeship | +1 per Mine, +1 per Quarry, +2 Aqueduct/Canal/Dam, +1 per 2 districts, +1 per 2 Lumber Mills | Workshop (+3P) → Factory (+3P regional to cities within 6 tiles; Power [GS]) → Power Plant (Coal/Oil/Nuclear [GS]; regional Power and production) |
| Encampment | Bronze Working | none; cannot be adjacent to City Center | Barracks (melee XP) OR Stable (cavalry XP) → Armory (+XP, stockpile cap) → Military Academy (+XP, allows Corps/Army training directly) |
| Entertainment Complex | Games and Recreation | none | Arena (+1 Amenity; Tourism with Sports) → Zoo (+1 Amenity regional 6 tiles) → Stadium (+2 Amenity regional, Power [GS]) |
| Water Park | Radio | none; on coast/lake adjacent land | Ferris Wheel → Aquarium → Aquatics Center (regional amenities) |
| Aqueduct | Engineering | Must be adjacent to City Center and a fresh water source (river/lake/oasis/mountain) | none (housing; +2 adjacency to Industrial Zone; prevents drought [GS]) |
| Neighborhood | Urbanization | none | Food Market / Shopping Mall (Power [GS]); housing by appeal |
| Aerodrome | Flight | none | Hangar → Airport (air unit capacity, airlift) |
| Spaceport | Rocketry | none, flat land | Space race projects |
| Government Plaza [R&F] | State Workforce | +1 adjacency to all districts adjacent to it | Tier 1/2/3 Government buildings (one per tier, e.g., Ancestral Hall, Audience Chamber, Warlord's Throne; Grand Master's Chapel, Intelligence Agency, Foreign Ministry; Queen's Bibliotheque, Royal Society, War Department...) |
| Diplomatic Quarter [GS] | Mercenaries (tunable) | none | Consulate → Chancery (diplomatic favor, spy defense, envoy) |
| Canal [GS] | Mass Production | Connects water bodies or coast to City Center | lets naval units cross land |
| Dam [GS] | Buttress | On floodplains across a river | Hydroelectric Dam (Power); prevents floods |
| Preserve [GS] | Mysticism | Uses appeal; boosts adjacent tile yields | Grove, Sanctuary |

Unique districts replace the generic one with extra effects (examples): Acropolis (Greece), Bath (Rome), Hansa (Germany), Lavra (Russia), Mbanza (Kongo), Royal Navy Dockyard (England), Street Carnival (Brazil), Ikanda (Zulu), Seowon (Korea), Cothon (Phoenicia), Hippodrome (Byzantium), Observatory (Maya), Oppidum (Gaul), Thanh (Vietnam), Suguba (Mali), Copacabana (Brazil).

### Example adjacency computation
```
def campus_adjacency(plot):
    s = 0
    for n in neighbors(plot):
        if n.terrain == MOUNTAIN: s += 1
        if n.feature in (REEF, GEOTHERMAL_FISSURE): s += 2
        if n.natural_wonder == GREAT_BARRIER_REEF: s += 2
        rainforest += n.feature == RAINFOREST
        districts += n.has_district  # City Center counts as a district; wonders do not
    s += rainforest // 2 + districts // 2
    return s * policy_multiplier
```

## Buildings
- Built inside a district (or City Center). Each has: production cost, gold maintenance, prerequisites (tech/civic, previous building), mutual exclusions (Barracks vs Stable, Art Museum vs Archaeological Museum), yields, housing, amenities, great work slots, citizen slots, GPP, regional effect radius (Factory, Zoo, Stadium, Power Plant: 6 tiles; regional effects don't stack from same building type), power requirement [GS].
- Notable non-district buildings: Monument (+2C; +1 loyalty [R&F]), Granary (+1F, +2 Housing), Water Mill (+1F, +1P, rice/wheat/maize +1F), Walls (defense), Sewer (+2 Housing), Palace.
- Power [GS]: Industrial and later buildings require Power (from power plants, dams, renewables, city-state). Unpowered buildings give reduced effects. Each powered building consumes N power (e.g., Factory 3, Research Lab 3, Stadium 3, Stock Exchange 3, Broadcast Center 3; tunable); total city power = sources within 6-tile regions + local sources. Burning fossil fuels emits CO2 → climate change.

## Wonders
- Each World Wonder can be built once in the world. Requirements: tech/civic, placement (terrain adjacency, e.g., Great Lighthouse on coast adjacent to Harbor; Petra on desert/floodplains; Colosseum on flat land adjacent to Entertainment Complex; Machu Picchu on mountain), not on resources or districts.
- When another civ completes a wonder you're building: the wonder is removed from your build options and the production already invested carries over to the next item the city builds (no gold refund).
- Wonders provide: yields, great work slots, free units, era score, tourism (+2 per wonder, raised by policies and Cristo-style effects; tunable), appeal +1 to neighbors.

Wonder list (era, key effect; costs are data):
| Wonder | Era | Effect summary |
|---|---|---|
| Stonehenge | Ancient | Grants free Great Prophet (founds religion) |
| Hanging Gardens | Ancient | +15% growth in all cities; +1 housing |
| Pyramids | Ancient | +2C; free Builder; all Builders +1 charge |
| Oracle | Ancient | Patronizing Great People with Faith costs 25% less; districts in this city give +2 GPP of their type |
| Temple of Artemis | Ancient | +4F, +3 Housing; each Camp, Pasture and Plantation within 6 tiles gives +1 Amenity |
| Great Bath [GS] | Ancient | +3 Housing, +1 Amenity; floodplains +1 Faith; floods on this river cause no damage |
| Etemenanki [GS] | Ancient | +2S; marsh/floodplains +1S/+1P |
| Great Lighthouse | Classical | +1 movement naval melee; +3G; Great Admiral points |
| Colossus | Classical | +3G, +1 trade route capacity, free Trader |
| Petra | Classical | Desert tiles +2F +2G +1P in the city |
| Colosseum | Classical | +2C; +2 Amenities and +2 Loyalty to cities within 6 tiles |
| Great Library | Classical | +2S, +1 Great Scientist point; grants all Ancient/Classical tech boosts; 2 Writing slots |
| Mausoleum at Halicarnassus | Classical | Great Admirals and Great Engineers get +1 charge; harbor +1 S/F |
| Terracotta Army | Classical | All land units gain a promotion level; archaeologists can enter foreign territory |
| Apadana | Classical | +2 Envoys when you build a wonder in this city |
| Jebel Barkal [DLC] | Classical | Grants 2 Iron; +4 Faith to your cities within 6 tiles |
| Machu Picchu | Renaissance (tunable) | Built on a Mountain; +4G; Commercial Hub, Theater Square and Industrial Zone get +1 adjacency per adjacent Mountain |
| Hagia Sophia | Medieval | +4 Faith; Missionaries and Apostles get +1 spread charge |
| Alhambra | Medieval | +1 Military policy slot; +2 Amenities; +2 Great General points |
| Chichen Itza | Medieval | Rainforest +2C +1P |
| Mont St. Michel | Medieval | Apostles get Martyr promotion |
| Kilwa Kisiwani [R&F] | Medieval | +15% yields from city-states you're suzerain of |
| Forbidden City | Renaissance | +1 Wildcard policy slot |
| Great Zimbabwe | Renaissance | +5G, +1 Trade Route capacity; trade routes from this city +2G per bonus resource in its territory (tunable); must be adjacent to Cattle and a Commercial Hub with Market |
| Potala Palace | Renaissance | +1 diplomatic slot; mountain |
| Venetian Arsenal | Renaissance | Two naval units per naval unit trained |
| Big Ben | Industrial | Doubles the gold in your treasury on completion; +1 Economic policy slot |
| Hermitage | Industrial | 4 art slots |
| Bolshoi Theatre | Industrial | Grants 2 random civics; 1 Writing and 1 Music slot |
| Oxford University | Industrial | 2 free techs; +20% science in city |
| Ruhr Valley | Industrial | +20% production in city; mines/quarries +1P |
| Statue of Liberty [R&F] | Industrial | +4 diplomatic favor; cities don't lose loyalty |
| Eiffel Tower | Modern | +2 appeal to all tiles |
| Broadway | Modern | +20% culture in city; grants a random Atomic-era civic; 3 Music slots |
| Cristo Redentor | Modern | Tourism from relics/holy cities not reduced; seaside resorts doubled |
| Estádio do Maracanã | Atomic | +2 amenities all cities |
| Golden Gate Bridge [GS] | Atomic | +3 Amenities in this city (tunable); acts as a land bridge across the water tile it occupies |
| Sydney Opera House | Atomic | 3 music slots, +8C |
| Panama Canal [GS] | Industrial | Canal + coastal connection |
| Amundsen-Scott [GS] | Modern | Snow/tundra production |
| Biosphère [GS] | Information | Renewable energy bonuses |

All wonder data must be tunable; text above is a guide to the *kind* of effect each provides. Final list in game: ~55 wonders.

## Builders and improvements
- **Builder** unit: civilian, 3 charges (Pyramids/policies +1/+2; Ilkum +30% production). Each improvement or harvest consumes 1 charge; repairing a pillaged improvement or district costs no charge. Removing features (chop/drain) costs 1 charge. Builder destroyed at 0 charges.
- Building is instant (same turn) and uses the builder's remaining movement.
- Military Engineers: build roads, railroads, forts, airstrips, missile silos, canals' progress, dams' progress, mountain tunnels; charges (2, Military Academy +1).

| Improvement | Unlock | Valid on | Yield | Notes |
|---|---|---|---|---|
| Farm | — | flat grass/plains/floodplains; hills w/ Civil Engineering | +1F | +1F per 2 adjacent farms (Feudalism), +1F per adjacent farm (Replaceable Parts); +0.5 Housing |
| Mine | Mining | hills, iron/coal/niter/etc. | +1P (+1 Apprenticeship, +1 Industrialization, +1 Smart Materials) | appeal −1 |
| Quarry | Mining | stone, marble, gypsum | +1P | |
| Pasture | Animal Husbandry | cattle, sheep, horses | +1P | +0.5 housing; Stirrups +1F (tunable) |
| Plantation | Irrigation | bananas, citrus, cocoa, coffee, cotton, dyes, silk, spices, sugar, tea, tobacco, wine | +2G (tunable) | +0.5 housing |
| Camp | Animal Husbandry | deer, furs, ivory, truffles | +1G | +0.5 housing |
| Fishing Boats | Sailing | fish, crabs, whales, pearls, turtles | +1F | +0.5 housing |
| Lumber Mill | Machinery | woods | +2P | Steel +1 |
| Oil Well / Offshore Platform | Refining / Plastics | oil | +2P | |
| Fort | Siege Tactics | any land | — | +4 defense, auto-fortify |
| Airstrip | Flight | flat | — | 3 air units |
| Missile Silo | Rocketry | — | — | stores nuclear weapons |
| Seaside Resort | Radio | coast-adjacent, Charming+ appeal | Tourism = appeal, Gold | |
| Ski Resort [GS] | Radio | mountain | Tourism, amenity | |
| Solar Farm / Wind Farm / Offshore Wind / Geothermal Plant [GS] | Industrial+ | specific terrain | Power | no CO2 |
| Mountain Tunnel [GS] | Chemistry | mountains | movement | |

Unique improvements: Ziggurat (Sumeria, +2S), Sphinx (Egypt, +1Fa +1C), Stepwell (India, +1F +1Fa housing), Château (France), Great Wall (China), Kurgan (Scythia), Mission (Spain), Hacienda (Gran Colombia), Pairidaeza (Persia), Golf Course (Scotland), Monastery (Armagh city-state), Polder (Netherlands), Outback Station (Australia), Kampung (Indonesia), Terrace Farm (Inca), Mekewap (Cree), Nubian Pyramid (Nubia), Chemamull (Mapuche). Each: unlocking tech/civic, placement rules, yields scaling with adjacency and techs.

## Projects
Repeatable or one-time production items run in districts:
- District projects (convert production to yield + GPP): Campus Research Grants, Holy Site Prayers, Theater Square Festival, Commercial Hub Investment, Harbor Shipping, Industrial Zone Logistics, Encampment Training, Entertainment Complex and Water Park "Bread and Circuses" (loyalty & amenities). Carnival is Brazil's Street Carnival/Copacabana replacement project.
- Space race (Spaceport): Launch Earth Satellite, Launch Moon Landing, Launch Mars mission(s)/Exoplanet Expedition [GS], Lagrange Laser Station [GS], Terrestrial Laser Station [GS].
- [GS]: Carbon Recapture, Flood Barrier, Decommission Power Plant, Repair Outer Defenses, Recommission Reactor.
- Nuclear: Manhattan Project (unlocks Nuclear Devices for the builder only), Operation Ivy (thermonuclear), Build Nuclear Device / Thermonuclear Device.
