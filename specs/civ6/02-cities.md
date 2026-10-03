# 02 Cities

## Founding
- A **Settler** founds a city on its current tile (consumes the Settler). Invalid on: water, mountains, natural wonders, oases, and tiles within **3 tiles** of another city center (minimum center distance 4). Resource tiles are valid: the resource is kept and the city center improves it automatically (luxury and strategic copies are granted).
- First city becomes the **Capital** and receives the **Palace** (building: +2P, +2S, +5G, +1C, +1 Housing, 1 Great Work slot; capital gets loyalty and +combat bonuses).
- Founding claims the center tile and its 6 neighbors (radius 1). The city center is effectively a district (City Center) that can hold buildings: Monument, Granary, Water Mill, Ancient Walls, Medieval Walls, Renaissance Walls, Sewer, plus unique buildings.
- City center tile yields: terrain yields with minimum **2F 1P** (raised to minimum), computed as `max(terrain_food,2)`, `max(terrain_prod,1)` (tunable), plus improved resource yields; city centers on a river get freshwater housing.
- New cities start with population 1. Cities are named from the civ's name list.
- City-ownership counts: original capital (can't be razed), captured cities (can be razed unless original capital), free cities [R&F].

## Workable area and borders
- Workable radius: 3 tiles from the center (37 tiles). Owned territory can extend to 5 tiles but tiles at 4–5 cannot be worked.
- **Border growth by culture**: each city accumulates its culture output into a border bucket; when the bucket reaches the cost of the next tile, the "best" adjacent unowned tile (scored by yields/resources) is claimed. Cost ≈ `10 + (6 × n)^1.3` culture where n = tiles acquired by growth (tunable).
- **Tile purchase**: gold cost based on distance and the same counter (≈ `distance-weighted base × (1 + tiles purchased)`); tiles with luxuries/strategics cost more.
- Building a district or wonder on an unowned adjacent tile is not allowed; on owned tiles only. Completing a district does not claim tiles by default. Culture bombs are a modifier `CULTURE_BOMB` (on completion, claim all unowned tiles adjacent to the district/improvement), used by civ abilities: Poland (Encampment, Fort), Australia (Pasture), Netherlands (Harbor, Dam), Gaul (Mine). Cree's Traders claim unowned tiles within 3 tiles of a Cree city on first entry.

## Population and food
- Each citizen consumes **2 Food**. Surplus food fills the growth bucket.
- Growth threshold for next pop: `15 + 8n + n^1.5` where `n = current_pop − 1` (×game speed).
- On growth, a portion of the bucket may be retained (Granary-like effects; base 0%).
- Starvation: negative surplus drains the bucket; at 0 the city loses 1 population.
- Growth modifiers (applied to surplus): Housing penalty and Amenity state (below), plus civ/policy bonuses. Final surplus = (food − consumption) × (1 + amenity_growth_mod) × housing_mult.

## Housing
- Base housing from city center water access: **2** no water, **3** coastal (adjacent to coast), **5** fresh water (river/lake/oasis).
- Additional sources: Palace +1; Granary +2; Water Mill +0 (it gives food/production, not housing); Aqueduct (no-water city: sets to 6 total; fresh water city +2); Sewer +2; Lighthouse +1; Farms/Pastures/Plantations/Camps/Fishing Boats +0.5 each; Neighborhoods +2 to +6 by appeal (Disgusting 2 … Breathtaking 6); Mbanza and other uniques; University +1; policy cards (e.g., Insulae +1 per city with 2+ districts), Great People, wonders (e.g., Temple of Artemis +3, Great Bath +3, Hanging Gardens +1 in all cities).
- Housing multiplier on growth:
  - `pop < housing − 1`: ×1
  - `pop == housing − 1`: ×0.5
  - `pop ≥ housing`: ×0.25
  - `pop ≥ housing + 5`: ×0 (growth stops)

## Amenities
- Required amenities: `max(0, ceil(pop / 2) − 1)` (pop 1–2: 0, 3–4: 1, 5–6: 2, ...).
- Sources: luxury resources (each copy +1 to up to 4 cities), Entertainment Complex and its buildings (Arena +1, Zoo +1 regional, Stadium +2 regional), Water Park buildings, Temple (with religion beliefs), policy cards (e.g., Retainers: +1 per garrisoned city; Liberalism +1 in cities with 2+ districts), national parks, great people, wonders (Colosseum +2 in radius), city-state suzerain bonuses, civic/tech unlocks, Governor Pingala/Amani effects, Golden Ages.
- Penalties: war weariness (accumulated from combat, decays in peace) and bankruptcy (negative treasury). Civilian units and religion cause no amenity penalty.
- Mood levels from `balance = amenities − required` (tunable table):

| State | Balance | Growth | Non-food yields | Other |
|---|---|---|---|---|
| Ecstatic | ≥ +3 | +20% | +10% | +loyalty [R&F] |
| Happy | +1..+2 | +10% | +5% | |
| Content | 0 | 0 | 0 | |
| Displeased | −1..−2 | −15% | −5% | |
| Unhappy | −3..−4 | −30% | −10% | cannot train Settlers |
| Unrest | −5..−6 | no growth | −15% | rebels may spawn |
| Revolt | ≤ −7 | no growth | −20% | rebels spawn, loyalty loss |

## Loyalty [R&F]
- Each city has Loyalty 0–100. Displayed per turn change.
- Inputs per turn:
  - **Population pressure**: every city within 9 tiles exerts pressure = its population × (1 − 0.1 × distance); the capital has no pressure multiplier (it gets the flat Capital bonus below). Pressure from own cities is positive; from others negative. Net = your weighted pressure − foreign pressure, scaled into roughly ±20/turn.
  - Governor stationed: +8.
  - Garrisoned unit: with policies (Limitanei +2; Praetorium etc.).
  - Amenity state: Ecstatic +2, Happy +1, Content 0, Displeased −1, Unhappy −2, Unrest −3, Revolt −6 (tunable).
  - Age: Golden Age +5, Dark Age −5, Normal 0 (applies to all cities, scaled).
  - Capital: +8.
  - Religion: no base effect; beliefs and policies may add loyalty via modifiers.
  - Monument: +1 loyalty [R&F].
  - Occupied city (captured from enemy, no garrison): −5 (tunable). Starving city: −5 (tunable).
- Loyalty ≤ 0: city revolts, becomes a **Free City** (independent, has its own units). Free cities can be won back by loyalty pressure (flips to the civ exerting most pressure) or conquered.
- Loyalty states and yield penalties: Loyal 76–100 none; Wavering 51–75 −25% yields; Disloyal 26–50 −50% yields; Unrest 1–25 −100% non-food yields and no growth (tunable).

## Production
- City accumulates production into the current item. Overflow carries to next item (capped at item cost or similar).
- Queue: up to ~8 items.
- Item types: units, buildings, districts, wonders, projects (district projects like Campus Research Grants, Carnival, space race projects, Bread and Circuses, Flood barrier [GS], Carbon recapture [GS], Encampment Training).
- **Purchasing**: gold or faith.
  - Gold purchase cost ≈ production cost × ~4 (buildings, units). Districts, wonders and projects cannot be bought.
  - Faith purchase: religious units always; Holy Site buildings (Shrine, Temple, worship building) always; land military units only with Theocracy, the Grand Master's Chapel, or certain beliefs.
  - Unit purchase cost is flat: it does not rise with the number of units of that type you own (Settlers and Builders scale via their production cost, below).
- Production modifiers: policies (e.g., Agoge +50% melee/ranged, Maritime Industries +100% naval melee... Ilkum +30% builders, Colonization +50% settlers, Corvée +15% ancient/classical wonders), civ abilities, Industrial zone regional buildings.
- Chopping: Builder harvests woods/rainforest/bonus resource: instant yield (~+20 P scaled by era/game speed; base: harvest yields scale with number of techs/civics completed).
- Unit costs: Settler base 80 production, +30 for each Settler already trained (tunable). Training a Settler reduces pop by 1 and requires pop ≥ 2.
- Builder cost increases with number already trained (~+4 per).

## Citizens and specialists
- Population works tiles (one citizen per tile) or acts as specialists inside districts (slots = number of buildings in the district; each specialist gives district-type yields: Campus 2S, Holy Site 2Fa, Theater 2C, Commercial Hub 4G, Industrial Zone 2P, Harbor 2G 1F, Encampment 1P 2G (tunable)).
- Automatic assignment by city focus (balanced default, or Food/Production/Gold/Science/Culture/Faith emphasis). Player can lock tiles.

## City combat
- City Center has **200 HP** (+ district HP for encampments). **Walls** add a separate HP/defense pool: Ancient Walls +100 HP, Medieval +100, Renaissance +100 (each requires the previous). Walls give the city a **ranged attack** (range 2; Encampment district also gets ranged strike with walls tech).
- City defense strength: based on strongest melee unit strength the player can build (≈ strongest melee −10) + walls bonus (+3 per wall level) + Palace bonus (+3 in the capital) + district bonus (+2 per specialty district in the city) (all tunable).
- Damage: attacks hit walls first. Non-siege ranged and melee units deal 50% damage to walls (tunable); siege units deal full damage; a Battering Ram gives adjacent melee full damage vs walls; a Siege Tower lets melee bypass walls and damage city HP directly.
- Capture: when city HP (center) is 0, a melee or anti-cav/light/heavy cav unit moving in captures it. Wall HP does not need to be 0 (in practice walls fall first because damage hits them first).
- Heal: city heals ~20 HP/turn if not attacked; walls do not heal on their own (repaired via production project / builder, or heal when not attacked for X turns depending on rules—choose: walls repair only via "Repair Walls" project unless not damaged for 3 turns).
- City can be **garrisoned**: one military unit may occupy the city center. It does not add to city strength; it fights from the city and enables garrison bonuses from policies.
- Captured city options: Keep (becomes occupied; war weariness, amenity −) / Raze (reduce 1 pop/turn until 0; original capitals cannot be razed) / Liberate (return to original owner, grievance −/diplomatic favor +).
- Encampment and City Center can also be surrounded: if all adjacent tiles are enemy-controlled, city can't heal.

## Trade routes origin/destination
See economy file. Each city can be a trade destination; Traders also build roads.

## Pillaging and districts in war
- Enemy units can pillage districts (except City Center) and improvements: gain plunder (gold/science/culture/faith depending on district) and heal. Pillaged districts' buildings stop working until repaired by Builder (district repair via city production).
