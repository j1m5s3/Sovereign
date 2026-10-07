# 05 Units, Movement, Combat

Reference data: per-unit stats in [data/units.md](data/units.md), promotion trees in [data/promotions.md](data/promotions.md), engine constants in [data/global-parameters.md](data/global-parameters.md). Values below are the Gathering Storm ruleset at Standard speed unless tagged otherwise; keep them in data, not code.

## Unit model
```
UnitType {
  id, class, era, domain (LAND | SEA | AIR),
  cost_production, cost_progression (model, param), purchase_yield (GOLD | FAITH),
  maintenance_gold, strategic_cost {resource, amount} [GS: Units_XP2.ResourceCost],
  strategic_maintenance {resource, per_turn} [GS],
  combat (melee CS), ranged_strength, bombard_strength, anti_air_strength, range, sight, movement,
  zone_of_control (bool), air_slots (carriers),
  spread_charges, religious_strength, religious_heal_charges, evict_percent (religious),
  build_charges (builders/engineers),
  abilities [ability ids, mostly granted via class tags], promotion_class, upgrades_to,
  obsolete_tech/civic, required_tech/civic, unique_to_civ, replaces
}
Unit instance { id, type, owner, plot, hp (0..100), moves_left, xp, level,
                promotions[], formation (single|corps|army), charges, fortify_turns,
                embarked, activity (awake|sleep|fortify|alert|heal|skip|automated) }
```

## Stacking (1UPT)
- Per tile, at most one unit per **layer**: military (land or naval combat unit), civilian (Settler, Builder, Trader, Great Person, Archaeologist, Naturalist, religious units), support (Battering Ram, Siege Tower, Military Engineer, Medic, Observation Balloon, Drone, Anti-Air Gun, Mobile SAM, Supply Convoy). Air units are based in cities/Aerodromes/airstrips/carriers and do not occupy map layers.
- Units of different owners never share a tile. A civilian on a tile with a friendly military unit is "escorted"; units can be **linked** to move together.
- Moving a military unit into a tile holding only an enemy civilian captures it. Capture results come from `UnitCaptures` (data): a captured Settler stays a Settler and a Builder stays a Builder (see [data/units.md](data/units.md#unit-capture-results)). Other civilians that are not capturable (e.g., Traders) are destroyed/plundered. Religious units can only be removed by military units at war via "Condemn Heretic" (see 06).

## Unit classes and representative units
Format: `Name CS[/RS] (range r, moves M, cost, resource [GS])`. Only the generic line per class is shown; uniques and full columns (sight, maintenance, obsolescence) are in [data/units.md](data/units.md). Every generic unit upgrades to the next one listed in its row (data: UnitUpgrades), except where noted.

| Class | Upgrade line |
|---|---|
| Recon | Scout 10 (M3, 30) → Skirmisher [GS] 20/30 (r1, M3, 150) → Ranger 45/60 (r1, M3, 380) → Spec Ops 60/65 (r2, M3, 520; paradrop) |
| Melee (+5 vs anti-cavalry, ability Anti-Spear) | Warrior 20 (M2, 40) → Swordsman 35 (90, Iron 20) → Man-at-Arms [GS] 45 (160, Iron 20) → Musketman 55 (240, Niter 20) → Line Infantry 65 (360, Niter 20) → Infantry 75 (430, Oil 1 + 1/turn) → Mechanized Infantry 85 (M3, 650, Oil 1 + 1/turn) |
| Anti-cavalry (+10 vs light, heavy and ranged cavalry) | Spearman 25 (65) → Pikeman 45 (180) → Pike and Shot [GS] 55 (250) → AT Crew 75 (400) → Modern AT 85 (M3, 580) |
| Ranged (−17 vs districts) | Slinger 5/15 (r1, 35) → Archer 15/25 (r2, 60) → Crossbowman 30/40 (r2, 180) → Field Cannon 50/60 (r2, 330) → Machine Gun 70/85 (r2, 540) |
| Light cavalry (ignores ZOC) | Horseman 36 (M4, 80, Horses 20) → Courser [GS] 46 (M5, 200, Horses 20) → Cavalry 62 (M5, 330, Horses 20) → Helicopter 86 (M4, 600, Aluminum 1 + 1/turn; ignores terrain cost and river crossing) |
| Heavy cavalry (ignores ZOC) | Heavy Chariot 28 (M2, 65; +1 movement when it starts the turn on flat open terrain) → Knight 50 (M4, 220, Iron 20) → Cuirassier [GS] 64 (M4, 330, Iron 20) → Tank 85 (M4, 480, Oil 1 + 1/turn) → Modern Armor 95 (M4, 680, Oil 1 + 1/turn) |
| Siege (CS / bombard; cannot attack after moving) | Catapult 25/35 (r2, 120) → Trebuchet 35/45 (r2, 200) → Bombard 45/55 (r2, 280, Niter 20) → Artillery 60/80 (r2, 430, Oil 1 + 1/turn) → Rocket Artillery 70/100 (r3, M3, 680, Oil 1 + 1/turn) |
| Naval melee | Galley 30 (M3, 65) → Caravel 55 (M4, 240) → Ironclad 70 (M5, 380, Coal 1 + 1/turn) → Destroyer 85 (M4, 540, Oil 1 + 1/turn; anti-air 90) |
| Naval ranged | Quadrireme 20/25 (r1, M3, 120) → Frigate 45/55 (r2, M4, 280, Niter 20) → Battleship 60/70 (r3, M5, 430, Coal 1 + 1/turn) → Missile Cruiser 75/90 (r3, M5, 680, Oil 1 + 1/turn) |
| Naval raider (stealth, coastal raid, ignores ZOC) | Privateer 40/50 (r2, M4, 280) → Submarine 65/75 (r2, M3, 480, Oil 1 + 1/turn) → Nuclear Submarine 80/85 (r2, M4, 680; can carry WMDs) |
| Naval carrier | Aircraft Carrier 65 (M3, 540; 2 air slots, more with promotions) |
| Air fighter | Biplane 80/75 (r4, 430, Oil 1 + 1/turn) → Fighter 100/100 (r5, 520, Aluminum) → Jet Fighter 110/110 (r6, 650, Aluminum) |
| Air bomber (bombard 110/120; can deliver WMDs) | Bomber 85, BS 110 (r10, 560, Aluminum 1 + 1/turn) → Jet Bomber 90, BS 120 (r15, 700, Aluminum 1 + 1/turn) |
| Support (no combat strength) | Battering Ram (65) → Siege Tower (100) → Medic (370) → Supply Convoy (450); Observation Balloon (240) → Drone (420); Anti-Air Gun (AA 90, 455) → Mobile SAM (AA 100, 590); Military Engineer (170, 2 build charges, needs Armory) |
| Giant Death Robot | 130 / RS 120 (r3, M5, AA 90, 1500, Uranium 1 + 3/turn); fights while embarked, resists WMDs, promotes automatically |
| Civilians | Settler, Builder, Trader, Archaeologist, Naturalist, Great People, Missionary, Apostle, Inquisitor, Guru, Spy (handled as an off-map unit while on mission) |

Note: in Gathering Storm the Aircraft Carrier and Nuclear Submarine have no strategic resource requirement (the expansion clears it); full stats are in [data/units.md](data/units.md).

Unique units replace generic ones and are listed with their abilities in [data/units.md](data/units.md). Examples: Roman Legion (40, one build charge for a Roman Fort or to clear a feature), Greek Hoplite (+10 when next to another Hoplite), Egyptian Maryannu Chariot Archer, Scythian Saka Horse Archer, English Redcoat, Zulu Impi (stronger flanking), Mongol Keshig, Japanese Samurai, Aztec Eagle Warrior (can capture defeated units as Builders), American P-51 Mustang, Indian Varu (−5 to adjacent enemies), Sumerian War-Cart, Norwegian Berserker, Chinese Crouching Tiger, Spanish Conquistador, Brazilian Minas Geraes, Kongo Ngao Mbeba, French Garde Impériale, German U-Boat, Russian Cossack, Arabian Mamluk.

## Movement
- Each unit has Movement Points (MP). Entering a tile costs the terrain cost (flat 1, hills 2) plus feature costs (woods, rainforest, marsh +1), so woods on hills costs 3. Mountains are impassable (data: Terrains/Features). Roads replace terrain cost (Ancient/Medieval road 1, Industrial 0.75, Modern 0.5, railroad 0.25; data: Routes).
- A unit that still has its full MP can always move one tile, whatever the cost; otherwise it needs enough remaining MP (engine; source: https://civilization.fandom.com/wiki/Movement_(Civ6)).
- **River crossing** costs 2 MP plus the destination tile's cost (data: GlobalParameters.MOVEMENT_RIVER_COST = 2). Roads from the Medieval era on build bridges that remove the crossing cost (data: Routes.SupportsBridges). Amphibious promotion removes it.
- **Zone of Control (ZOC)**: units with `ZoneOfControl = true` (melee, anti-cavalry, cavalry and most naval units; ranged, siege and air units do not), city centers and Encampments (data: Districts.ZOC) exert ZOC on adjacent tiles. A unit that enters a tile in enemy ZOC loses its remaining MP (engine; source: https://civilization.fandom.com/wiki/Movement_(Civ6)). All light and heavy cavalry and naval raiders ignore ZOC (ability Ignore ZOC via class tags); some promotions and uniques grant it too.
- **Borders**: units cannot enter another major civ's territory without Open Borders, alliance or war. Borders are open to everyone until the owner completes Early Empire (data: CIVIC_ENFORCE_BORDERS). Units with an "enter foreign lands" ability ignore this: Missionaries, Apostles, Gurus, Traders, Rock Bands, Great People. City-states: entry allowed unless at war.
- **Embarkation**: all land units can embark after Shipbuilding (Builders already after Sailing, Traders after Celestial Navigation; data: Technologies.EmbarkUnitType/EmbarkAll). Ocean tiles need Cartography for both naval and embarked units. Embarking or disembarking costs 2 MP plus the destination tile's cost, or all remaining MP if the unit has less (data: GlobalParameters.MOVEMENT_EMBARK_COST = 2; source: https://civilization.fandom.com/wiki/Movement_(Civ6)). Embarked base movement 2 (MOVEMENT_WHILE_EMBARKED_BASE), +1 Square Rigging, +2 Steam Power, +1 Combustion (data: TechnologyModifiers). Embarked units defend with a fixed strength set by the owner's era (data: Eras.EmbarkedUnitStrength): Ancient 10, Classical 15, Medieval 15, Renaissance 30, Industrial 35, Modern 50, Atomic and later 55. Embarked units cannot attack unless an ability allows it (e.g., GDR).
- Pathfinding: A* with per-unit cost function; respects ZOC, borders, embark rules, visibility (known tiles only), and remaining-turn split.
- **Airlift**: Rapid Deployment civic allows moving land units between Aerodromes that have an Airport.
- Units with 0 MP can still fortify/sleep.

## Combat resolution
All damage values are integers; units have **100 HP** (COMBAT_MAX_HIT_POINTS). Cities and walls have their own HP (city center 200, Encampment 100; each wall level adds 100 outer-defense HP, see [data/buildings.md](data/buildings.md)).

### Strength calculation
```
effective_CS(unit, context) =
    base_strength (CS for melee/defence, RS for ranged attacks, BS for bombard attacks)
  + promotion bonuses
  + terrain defence (defender only): hills +3, woods/rainforest +3, reef +3, marsh/floodplains −2
      (data: Terrains/Features.DefenseModifier; hills and woods stack)
  − 5 if the attacker attacks across a river (COMBAT_RIVER_DEFENSE)
  − 10 if a land unit attacks from water / embarked (COMBAT_AMPHIBIOUS_ATTACK_PENALTY)
  + fortification (defender only): +3 per turn fortified, max 2 turns = +6 (FORTIFY_BONUS_PER_TURN, FORTIFY_TURN_MAX)
  + flanking: +2 per other friendly military unit adjacent to the defender (attacker only)
  + support: +2 per friendly military unit adjacent to the defender (defender only)
      (COMBAT_FLANKING_BONUS_MODIFIER, COMBAT_SUPPORT_BONUS_MODIFIER; both unlocked by the Military Tradition civic)
  + Great General / Admiral aura +5 (see below)
  + formation: Corps/Fleet +10, Army/Armada +17 (COMBAT_CORPS_/ARMY_STRENGTH_MODIFIER); vs air +7 for either
  + class matchups: anti-cavalry +10 vs cavalry; melee +5 vs anti-cavalry
  − 17 for ranged units (and air fighters) attacking a district or city (COMBAT_RANGED_VS_DISTRICT_STRENGTH_MODIFIER)
  − 17 for siege units and bombers attacking a unit (COMBAT_BOMBARD_VS_UNIT_STRENGTH_MODIFIER)
  + difficulty bonus (AI), civ/leader abilities, policies, beliefs (e.g., Crusade +10, Defender of the Faith +5), city-state envoys
  − wounded penalty: round(10 − hp/10)  (engine; source: https://civilization.fandom.com/wiki/Combat_(Civ6); data: COMBAT_WOUNDED_DAMAGE_MULTIPLIER = 10)
  − 20 if the owner lacks the strategic resource to pay this unit's maintenance [GS] (COMBAT_STRENGTH_REDUCTION_INSUFFICIENT_FUEL)
```

### Damage formula
```
damage_to_target = 30 × exp(0.04 × (S_attacker − S_target)) × rand(0.8, 1.2)
```
- Data backs every constant: COMBAT_POWER_SCALING = 0.04, and the random base damage is COMBAT_BASE_DAMAGE (24) + rand(0..COMBAT_MAX_EXTRA_DAMAGE = 12), i.e. 24–36 = 30 ± 20%. Minimum damage 1 (COMBAT_MINIMUM_DAMAGE). (Formula: engine; source: https://forums.civfanatics.com/threads/hans-lemurson-figures-out-the-combat-formula.606147/. That thread estimated ±25% randomness; the GlobalParameters give ±20%.)
- **Melee**: both sides take damage. The attacker's damage uses `S_att − S_def`; the defender's counter-damage uses `S_def − S_att` (each with its own roll). If the defender dies and the attacker survives, the attacker moves into the tile (a city is taken only when its HP reaches 0 and it is attacked by a melee unit).
- **Ranged** (ranged units, cities, Encampments, naval ranged, air): only the target takes damage. The attacker uses RS; the defender uses its melee CS. Needs line of sight unless the unit has an indirect-fire ability. Ranged class units get −17 against districts; naval ranged units do not.
- **Bombard**: siege units and bombers use bombard strength; full damage to districts and walls, −17 against units.
- **Walls**: while a city has outer-defense HP > 0, attacks damage the walls first. Damage to walls is scaled by attack type: melee 15%, ranged 50%, bombard 100% (data: COMBAT_DEFENSE_DAMAGE_PERCENT_MELEE/RANGED/BOMBARD). Melee attackers do full wall damage while a Battering Ram is adjacent to the city (Ancient Walls only); a Siege Tower adjacent lets melee attackers ignore the walls and hit the city directly (Ancient and Medieval Walls only) (engine limits; source: https://civilization.fandom.com/wiki/Battering_Ram_(Civ6)).
- Kill: HP ≤ 0 → unit destroyed. A Corps/Army is destroyed as a whole.
- Air combat: fighters on patrol intercept aircraft entering their range; anti-air units (and destroyers, battleships, missile cruisers) protect adjacent tiles with their anti-air strength.

### XP and promotions
- XP per combat (engine formula; source: https://forums.civfanatics.com/resources/civ-vi-experience.26777/): base = enemy base strength / own base strength (×2 if a unit was killed, EXPERIENCE_KILL_BONUS), +2 for melee combat or +1 for ranged combat, +1 for the attacker; capped at 8 per combat (data: EXPERIENCE_NOT_COMBAT_RANGED = 2, EXPERIENCE_COMBAT_RANGED = 1, EXPERIENCE_COMBAT_ATTACKER_BONUS = 1, EXPERIENCE_MAXIMUM_ONE_COMBAT = 8). Capturing a city 10 XP, attacking a city/district without capturing 3, district attacking a unit 2. Fights against barbarians cannot take a unit beyond level 2 (EXPERIENCE_MAX_BARB_LEVEL).
- Barracks, Stable, Armory and Military Academy (land) and Lighthouse, Shipyard, Seaport (naval) each give +25% XP to units trained or bought (with Gold or Faith) in that city, for good; the Hangar gives fighters and bombers +25% and the Airport +50%. Each covers only its own classes (the Barracks melee, ranged and anti-cavalry; the Stable light and heavy cavalry and siege; the Armory and Military Academy all six; data: [data/units.md](data/units.md), Unit abilities). The buildings of a pillaged district give nothing until it is repaired.
- Promotion threshold: 15 × current level XP for the next level (15, 30, 45, ...); excess XP is lost on promotion (engine; source: https://forums.civfanatics.com/resources/civ-vi-experience.26777/). Taking a promotion heals 50 HP (EXPERIENCE_PROMOTE_HEALED) and ends the unit's turn (engine; source: https://civilization.fandom.com/wiki/Promotion_(Civ6)).
- Each combat class has its own tree of 7 promotions in 4 tiers (two tier-1 options, two tier-2, two tier-3, one tier-4); a promotion needs any one of its listed prerequisites.
- Example (Melee): Battlecry (+7 when attacking melee, ranged or anti-cavalry) / Tortoise (+10 defending vs ranged) → Commando (scale cliffs, +1 movement) / Amphibious (no river-crossing penalty, no embark/disembark cost) → Zweihander (+7 vs anti-cavalry) / Urban Warfare (+10 melee combat in districts) → Elite Guard (+1 attack per turn, can move after attacking).
- Promotion classes: Recon, Melee, Ranged, Anti-Cavalry, Light Cavalry, Heavy Cavalry, Siege, Naval Melee, Naval Ranged, Naval Raider, Naval Carrier, Air Fighter, Air Bomber, Warrior Monk, Apostle, Rock Band, Spy, and Giant Death Robot (4 independent upgrades). Support units have no promotions. Full trees: [data/promotions.md](data/promotions.md).

### Healing (unit must not move or attack this turn)
- Land units per turn: enemy territory 5 HP, neutral 10, friendly 15, garrisoned in a city 20 (data: COMBAT_HEAL_LAND_ENEMY/NEUTRAL/FRIENDLY, COMBAT_HEAL_CITY_GARRISON). Naval units: 20 in friendly territory, 0 elsewhere (COMBAT_HEAL_NAVAL_*), unless a promotion or ability allows it. Medic and Supply Convoy: +20 HP to units within 1 tile. Promotions such as March let units heal after moving. Air units heal at their base.
- [GS] A unit whose strategic maintenance cannot be paid does not heal (STRATEGIC_RESOURCE_MINIMUM_FOR_UNIT_HEALING).

### Formations
- **Corps/Fleet** (Nationalism): merge 2 identical units (+10 CS). **Army/Armada** (Mobilization): merge 3, or a Corps plus 1 (+17 CS). A city with a Military Academy (land) or Seaport (naval) can train them directly; base production cost is ×1.5 for a Corps and ×2.0 for an Army (data: UNIT_CORPS_COST_MODIFIER, UNIT_ARMY_COST_MODIFIER), and those buildings add +25% production toward them.
- **Escort formation**: link civilian + military to move together.

### Upgrades
Pay gold (GS: also the target unit's strategic resource) to upgrade to the next unit in the line once its tech/civic is known. The unit keeps its promotions and XP and uses its turn. The unit must be in its owner's territory and have movement left (engine; unverified). Cost parameters: UPGRADE_BASE_COST 10, UPGRADE_MINIMUM_COST 15, UPGRADE_NET_PRODUCTION_PERCENT_COST 100 (exact formula: engine; unverified).

### Great Generals and Admirals
- Earned via GPP (see 07). Passive aura: +5 CS and +1 movement to friendly land units (Admirals: naval units) within 2 tiles whose unit era is the same as the Great Person's era or the next era (data: GreatPersonIndividualBirthModifiers, e.g. AOE_CLASSICAL_REQUIREMENTS covers Classical and Medieval units).
- Retire ability (one-time): each individual has its own effect (create a Corps, grant XP, build a Fort, etc.); see [data/great-people.md](data/great-people.md).

### War support systems
- **Pillage**: costs 3 MP (PILLAGE_MOVEMENT_COST; 1 with certain promotions). Yields come from data (Improvements/Districts.PlunderType, PlunderAmount): e.g., farm heals the unit 50 HP, mine 50 Gold, plantation/pasture/quarry/camp 25 Faith, Campus 25 Science, Holy Site 25 Faith, Theater Square 25 Culture, Commercial Hub 50 Gold. Pillaged tiles produce nothing until repaired.
- **Coastal raid**: naval melee and raiders pillage coastal improvements.
- **Plunder trade route**: a military unit entering a trade route's path while at war plunders it for gold.
- **Capture civilians**: see Stacking above.

## Nuclear weapons
- Manhattan Project (Nuclear Fission, 1000 production) unlocks building Nuclear Devices (project, 800 production, 10 Uranium). Operation Ivy (Nuclear Fusion) unlocks Thermonuclear Devices (1000 production, 20 Uranium). Maintenance 14 and 16 Gold per turn (data: WMDs.Maintenance).
- Delivery: Missile Silo or Nuclear Submarine (range 12 for Nuclear Device, 15 for Thermonuclear Device; data: WMDs.ICBMStrikeRange), or Bomber/Jet Bomber (uses the aircraft's range).
- Effects (data: WMDs): Nuclear Device blast radius 1, fallout 10 turns; Thermonuclear Device radius 2, fallout 20 turns. Units in the blast are destroyed (GDR immune), city centers and Encampments drop to 0 defence, improvements, districts and buildings are pillaged, and citizens working affected tiles are killed (engine; source: https://civilization.fandom.com/wiki/Nuclear_weapons_(Civ6)). Units ending a turn on a contaminated tile take 50 damage (PLOT_CONTAMINATION_DAMAGE_BASE); contaminated tiles cannot be worked. Launching adds war weariness (WAR_WEARINESS_PER_WMD_LAUNCHED 10) and can trigger an emergency [GS]. Nukes can only be used against a civ you are at war with (engine; unverified).

## Barbarian AI combat
Same rules; barbarian units do not earn promotions, and units fighting barbarians are capped at level 2 (see XP).
