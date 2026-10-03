# 02 Cities

Related data: [data/global-parameters.md](data/global-parameters.md) (CITY_*, CULTURE_*, PLOT_*, LOYALTY_*, IDENTITY_*, COMBAT_* parameters), [data/eras-moments-loyalty.md](data/eras-moments-loyalty.md) (amenity and loyalty levels), [data/buildings.md](data/buildings.md), [data/units.md](data/units.md).

## Founding
- A **Settler** founds a city on its current tile (consumes the Settler). Invalid on: water, mountains, natural wonders, oases, and any tile within **3 tiles** of another city center on the same landmass, so centers are at least 4 apart (data: CITY_MIN_RANGE = 3). If the two sites are separated by water (different landmass) the minimum drops to 3 apart (engine; source: https://civilization.fandom.com/wiki/City_(Civ6)). Resource tiles are valid: the resource is kept and the city center counts as improving it (luxury and strategic copies are granted).
- First city becomes the **Capital** and receives the **Palace** (building: +2P, +2S, +5G, +1C, +1 Housing, +2 Amenities, 1 Great Work slot, +3 city defense strength; data: Buildings, BuildingModifiers).
- Founding claims the center tile and its 6 neighbors (radius 1). The city center is effectively a district (City Center) that can hold buildings: Monument, Granary, Water Mill, Ancient Walls, Medieval Walls, Renaissance Walls, Sewer, plus unique buildings.
- City center tile yields: terrain yields raised to a minimum of **2F 1P**, i.e. `max(terrain_food, 2)`, `max(terrain_prod, 1)` (data: YIELD_FOOD_CITY_TERRAIN_REPLACE = 2, YIELD_PRODUCTION_CITY_TERRAIN_REPLACE = 1), plus resource yields; city centers next to a river, lake or oasis get fresh-water housing.
- New cities start with population 1. Cities are named from the civ's name list.
- City-ownership counts: original capital (can't be razed), captured cities (can be razed unless original capital), free cities [R&F].

## Workable area and borders
- Workable radius: 3 tiles from the center (37 tiles). Owned territory can extend to 5 tiles (data: PLOT_INFLUENCE_MAX_ACQUIRE_DISTANCE) but tiles at 4–5 cannot be worked.
- **Border growth by culture**: each city accumulates its culture output into a border bucket; when the bucket reaches the cost of the next tile, the best-scoring adjacent unowned tile is claimed. Cost = `10 + (6 × n)^1.3` culture, where n = tiles already acquired by culture growth (data: CULTURE_COST_FIRST_PLOT = 10, CULTURE_COST_LATER_PLOT_MULTIPLIER = 6, CULTURE_COST_LATER_PLOT_EXPONENT = 1.3; formula engine; source: https://forums.civfanatics.com/threads/formula-thread.600534/). Scale by game speed.
- Tile choice uses an "influence cost" score where lower is better (data: PLOT_INFLUENCE_*): +100 per ring of distance, +25 for water, −105 for a resource, −105 for a natural wonder, −5 for an improvement, −1 per yield point.
- **Tile purchase**: only tiles within 3 of the center (data: CITY_MAX_BUY_PLOT_RANGE = 3) adjacent to existing territory. Base price is 50 Gold (data: PLOT_BUY_BASE_COST) for tiles 2 away and 75 for tiles 3 away, and it rises with the share of techs and civics researched; resources do not change the price (engine; source: https://civilization.fandom.com/wiki/Borders_(Civ6)).
- Building a district or wonder on an unowned adjacent tile is not allowed; on owned tiles only. Completing a district does not claim tiles by default. Culture bombs are a modifier (data: MODIFIER_PLAYER_ADD_CULTURE_BOMB_TRIGGER; on completion, claim all unowned tiles adjacent to the district/improvement), used by: Poland (Encampment, Fort), Australia (Pasture), Gaul (Mine), Netherlands (Harbor), Māori (Fishing Boats), Khmer leader (Holy Site), the Burial Grounds and Warrior Monks beliefs (Holy Site), one Great Person (Industrial Zone), and every major civ's Preserve [GS]. Jadwiga's (Poland) leader trait additionally makes these culture bombs convert the affected city to her religion. Cree's Traders claim unowned tiles within 3 tiles of a Cree city on first entry.

## Population and food
- Each citizen consumes **2 Food** (data: CITY_FOOD_CONSUMPTION_PER_POPULATION). Surplus food fills the growth bucket.
- Growth threshold for next pop: `15 + 8n + n^1.5` where `n = current_pop − 1`, rounded down, then scaled by game speed (data: CITY_GROWTH_THRESHOLD = 15, CITY_GROWTH_MULTIPLIER = 8, CITY_GROWTH_EXPONENT = 1.5; definition of n engine; source: https://forums.civfanatics.com/threads/formula-thread.600534/). Pop 1→2 needs 15, 2→3 needs 24.
- On growth, a portion of the bucket may be retained (Granary-like effects; base 0%).
- Starvation: negative surplus drains the bucket; at 0 the city loses 1 population.
- Growth modifiers (applied to surplus): amenity state, loyalty level, housing, plus civ/policy/wonder bonuses (e.g. Hanging Gardens +15% in all cities). Occupied cities do not grow (data: CITY_GROWTH_OCCUPATION_MULTIPLIER = 0). Final surplus = (food − consumption) × (1 + sum of percentage growth modifiers) × housing_mult × loyalty_growth_mult.

## Housing
- Base housing from city center water access (data: CITY_POPULATION_*): **2** no water, **3** coastal (adjacent to coast), **5** fresh water (river/lake/oasis).
- Additional sources: Palace +1; Granary +2; Water Mill +0 (it gives food/production, not housing); Aqueduct (a city without fresh water is raised to 6 total; a fresh-water city gets +2; data: CITY_POPULATION_AQUEDUCT_MIN = 6, CITY_POPULATION_AQUEDUCT_BOOST = 2); Sewer +2; Lighthouse +1, plus +2 more if the city center is adjacent to coast; University +1; Farms/Pastures/Plantations/Camps/Fishing Boats +0.5 each; Neighborhoods 4 base adjusted by appeal (Disgusting 2, Uninviting 3, Average 4, Charming 5, Breathtaking 6; data: Districts.Housing + AppealHousingChanges); Mbanza 5; Rome's Bath 2; Zulu Ikanda 1; Dam 3 [GS]; Preserve 1 [GS]; policy cards (e.g., Insulae +1 in cities with 2+ specialty districts), Great People, wonders (e.g., Temple of Artemis +3, Great Bath +3, Hanging Gardens +2 in their own city).
- Housing multiplier on growth (data: CITY_HOUSING_LEFT_50PCT_GROWTH = 1, CITY_HOUSING_LEFT_25PCT_GROWTH = 0, CITY_HOUSING_LEFT_ZERO_GROWTH = −4, where "housing left" = housing − pop):
  - `pop ≤ housing − 2`: ×1
  - `pop == housing − 1`: ×0.5
  - `pop ≥ housing`: ×0.25
  - Growth stops once the city is 5 over its housing, i.e. it can reach `housing + 5` at most (engine; source: https://forums.civfanatics.com/threads/food-how-much-do-you-need.606382/).

## Amenities
- Required amenities: `max(0, ceil(pop / 2) − 1)` (pop 1–2: 0, 3–4: 1, 5–6: 2, ...) (data: CITY_POP_PER_AMENITY = 2; offset engine; source: https://civilization.fandom.com/wiki/Amenities_(Civ6)).
- Sources: luxury resources (each type +1 to up to 4 cities), Palace +2, Entertainment Complex buildings (Arena +2, Zoo +1 and Stadium +1 to every city within 6 tiles; Stadium +2 more when powered [GS]), Water Park buildings (Ferris Wheel +2, Aquarium and Aquatics Center +1 within 9 tiles), Aqueduct next to a geothermal fissure, unique districts (Brazil Street Carnival +2, Byzantium Hippodrome +3, Rome Bath +1), Dam +1 [GS], Temple (with religion beliefs), policy cards (e.g., Retainers: +1 in cities with a garrison; Liberalism +1 in cities with 2+ specialty districts), National Parks (+2 to the owning city and +1 to 4 other cities; data: NATIONAL_PARK_*), great people, wonders (Colosseum +2 amenities and +2 loyalty to cities within 6 tiles; Temple of Artemis: each Camp, Pasture and Plantation within 4 tiles gives +1 amenity to its city; Great Bath +1), city-state suzerain bonuses, civic/tech unlocks, Governor effects, Dedications.
- Penalties: war weariness (data: WAR_WEARINESS_*; 400 accumulated points cost 1 amenity) and bankruptcy (negative treasury: amenities lost in steps; data: GOLD_NEGATIVE_BALANCE_*). Civilian units and religion cause no amenity penalty.
- Mood levels from `balance = amenities − required` (data: Happinesses, Happinesses_XP1):

| State | Balance | Growth | Non-food yields | Loyalty/turn [R&F] | Rebellion points |
|---|---|---|---|---|---|
| Ecstatic | ≥ +5 | +20% | +20% | +6 | −1 |
| Happy | +3..+4 | +10% | +10% | +3 | −1 |
| Content | 0..+2 | 0 | 0 | 0 | −1 |
| Displeased | −1..−2 | −15% | −10% | −3 | −1 |
| Unhappy | −3..−4 | −30% | −20% | −6 | 0 |
| Unrest | −5..−6 | no growth | −30% | −6 | +1 |
| Revolt | ≤ −7 | no growth | −40% | −6 | +4 |

- Rebellion: rebellion points accumulate per turn by state (floored at 0); each point gives a 2% chance per turn of rebel units spawning near the city, with a 20-turn cooldown after a spawn (data: REBELLION_CHANCE_PER_POINT = 2.0, REBELLION_COOLDOWN_TURNS = 20; exact use engine; unverified).

## Loyalty [R&F]
- Each city has Loyalty 0–100 (data: LOYALTY_MAXIMUM), starting at 100. Displayed per turn change.
- Inputs per turn:
  - **Citizen pressure** (engine; source: https://forums.civfanatics.com/resources/civ-vi-loyalty-guide.27114/): every city within 9 tiles contributes `population × (10 − distance)` (data: CITIZEN_IDENTITY_PRESSURE_RADIUS_CUTOFF = 10). Sum separately for the owner's cities (domestic) and all other civs' cities (foreign), each multiplied by that civ's age factor: 1.0 normal, 1.5 Golden/Heroic Age, 0.5 Dark Age (data: GOLDEN_AGE_CITY_IDENTITY / DARK_AGE_CITY_IDENTITY = ±0.5 per citizen). Each civ's capital counts a second time at age factor 1.0. Net loyalty = `10 × (domestic − foreign) / (min(domestic, foreign) + 0.5)`, clamped to ±20 (data: LOYALTY_PER_TURN_FROM_NEARBY_CITIZEN_PRESSURE_MAX_LOYALTY = 20, MAX_RATIO = 3.0, so a 3:1 ratio hits the cap).
  - Governor stationed: +8 (engine; source above). Praetorium policy +2 more.
  - Garrisoned unit: only with policies (Limitanei +2, Martial Law +4; data: PolicyModifiers).
  - Amenity state: see the table above (Ecstatic +6, Happy +3, Displeased −3, Unhappy or worse −6).
  - Religion: city's majority religion is one the owner founded +3, a religion another civ founded −3 (data: IDENTITY_PER_TURN_FROM_RELIGION_*).
  - Starving city: −4 (data: IDENTITY_PER_TURN_FROM_STARVATION).
  - Monument: +1 (and +1 Culture while loyalty is full).
  - Wonders, Great Works, policies, governor titles and beliefs add more via modifiers.
  - Free Cities +10/turn and city-states +20/turn towards themselves (data: IDENTITY_PER_TURN_FROM_FREE_CITIES / _CITY_STATES).
- Loyalty 0: the city revolts and becomes a **Free City** (independent, has its own units). Free cities can be won back by loyalty pressure (they join the civ exerting the most pressure, arriving at 100 loyalty) or conquered. A city taken in combat starts at 50 loyalty; a liberated one at 100 (data: LOYALTY_AFTER_TRANSFERRED_BY_*).
- Loyalty levels (data: LoyaltyLevels):

| Level | Loyalty | Yields | Growth |
|---|---|---|---|
| Loyal | 76–100 | 0 | ×1 |
| Wavering | 51–75 | −25% | ×0.75 |
| Disloyal | 26–50 | −50% | ×0.25 |
| Unrest | 0–25 | −100% | ×0 |

## Production
- City accumulates production into the current item. Overflow carries to the next item (engine; exact cap unverified).
- Queue: a short list of items (engine; unverified length).
- Item types: units, buildings, districts, wonders, projects (district projects like Campus Research Grants, Carnival, space race projects, Bread and Circuses, Flood barrier [GS], Carbon recapture [GS], Encampment Training).
- **Purchasing**: gold or faith.
  - Gold purchase cost = 4 × production cost, rounded down to a multiple of 5 (engine; source: https://forums.civfanatics.com/threads/formula-thread.600534/). Districts, wonders and projects cannot be bought.
  - Faith purchase: religious units always; Holy Site buildings (Shrine, Temple, worship building) always; land military units only with Theocracy, the Grand Master's Chapel, or certain beliefs.
  - Unit purchase cost is flat: it does not rise with the number of units of that type you own (Settlers and Builders scale via their production cost, below).
  - [GS] Units that need a strategic resource also need the resource cost in the stockpile when trained or bought.
- Production modifiers: policies (e.g., Agoge +50% ancient/classical melee, ranged and anti-cavalry; Maritime Industries +100% ancient/classical naval; Ilkum +30% Builders; Colonization +50% Settlers; Corvée +15% ancient/classical wonders), civ abilities, Industrial Zone regional buildings.
- Chopping: Builder harvests woods/rainforest/marsh/bonus resource for an instant yield: base Woods 20P, Rainforest 10P + 10F, Marsh 20F, bonus resources 20 (40 for Gold), scaled by game progress up to ~10× and by game speed (see the map file).
- Unit costs (data: Units.CostProgressionModel = PREVIOUS_COPIES): Settler base 80 production, +30 for each Settler previously trained (engine counts previous copies). Training a Settler reduces pop by 1 and requires pop ≥ 2.
- Builder base 50, +4 for each Builder previously trained.

## Citizens and specialists
- Population works tiles (one citizen per tile) or acts as specialists inside districts (one slot per building in the district; each specialist gives district-type yields (data: District_CitizenYieldChanges): Campus 2S, Holy Site 2Fa, Theater 2C, Commercial Hub 4G, Industrial Zone 2P, Harbor 2G 1F, Encampment 1P 2G). Some buildings add +1 to their district's specialist yield.
- Automatic assignment by city focus (balanced default, or Food/Production/Gold/Science/Culture/Faith emphasis). Player can lock tiles.

## City combat
Mostly engine; source for the rules below: https://forums.civfanatics.com/resources/city-combat.27737/ unless a data value is given.
- City Center has **200 HP** (data: Districts.HitPoints; Encampments 100). **Walls** add a separate outer-defense HP pool: Ancient Walls 100, Medieval Walls +100, Renaissance Walls +100 (each requires the previous; data: Buildings.OuterDefenseHitPoints). Walls give the city a **ranged strike** at range 2 (data: Districts_XP2.AttackRange); an Encampment can strike too.
- City defense strength = max(combat strength of the strongest unit the player has built − 10, garrison unit strength) + 3 per wall level (data: OuterDefenseStrength = 3) + 3 for the Palace in the capital (data) + 2 per completed, non-pillaged specialty district (data: Districts.CityStrengthModifier) + 3 if on hills (terrain defense) + policy/promotion bonuses. Strength drops by 1 per 10% damage to the garrison/city HP.
- Damage: while the walls have HP, attacks hit the walls first. Melee units deal 15% damage to Ancient Walls and cannot damage Medieval or Renaissance Walls (data: COMBAT_DEFENSE_DAMAGE_PERCENT_MELEE = 15; CASTLE/STAR_FORT modifiers). Ranged units deal 50% (data: COMBAT_DEFENSE_DAMAGE_PERCENT_RANGED) and fight at −17 strength against districts (data: COMBAT_RANGED_VS_DISTRICT_STRENGTH_MODIFIER); naval ranged deals 50% but without the −17. Siege units deal full damage (data: _BOMBARD = 100). A Battering Ram gives adjacent melee full damage vs Ancient Walls; a Siege Tower lets melee bypass walls (Renaissance Walls cannot be bypassed).
- Capture: when city HP (center) is 0, a melee, anti-cavalry or cavalry unit moving in captures it. Ranged, siege, support, civilian and air units cannot capture. When captured, the city takes 50% damage and loses 25% of its population (data: CITY_CAPTURED_DAMAGE_PERCENTAGE, CITY_POPULATION_LOSS_TO_CONQUEST_PERCENTAGE).
- Heal: city HP heals 20 per turn (data: COMBAT_HEAL_CITY_GARRISON = 20) unless under siege. Walls do not heal on their own: after 3 full turns without being attacked (data: COMBAT_HEAL_OUTER_DEFENSES_COOLDOWN = 3) the city can run the Repair Outer Defenses project.
- City can be **garrisoned**: one military unit may occupy the city center. If its strength is higher than the city's derived value it sets the city's strength; it also enables garrison bonuses from policies.
- Captured city options: Keep (becomes occupied; does not grow while occupied) / Raze (original capitals cannot be razed; data: COMBAT_RAZE_ANY_CITY = 0) / Liberate (return to original owner; +100 Diplomatic Favor [GS], data: FAVOR_FOR_LIBERATE_*).
- Siege: if each of the 6 adjacent tiles is occupied by an enemy unit or in an enemy zone of control, the city is under siege and cannot heal.

## Trade routes origin/destination
See economy file. Each city can be a trade destination; Traders also build roads.

## Pillaging and districts in war
- Enemy units can pillage districts (except City Center) and improvements: gain plunder (gold/science/culture/faith depending on district) and heal. Pillaged districts' buildings stop working until repaired by Builder (district repair via city production).
