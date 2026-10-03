# 05 Units, Movement, Combat

## Unit model
```
UnitType {
  id, class, era, domain (LAND | SEA | AIR),
  cost_production, cost_gold_purchase_mult, cost_faith (optional),
  maintenance_gold, strategic_cost {resource, amount}, strategic_maintenance [GS],
  combat (melee CS), ranged_strength, bombard_strength, range, sight, movement,
  charges (builders/engineers/religious), religious_strength,
  abilities [modifier ids], promotion_class, upgrades_to, obsolete_tech/civic,
  required_tech/civic, unique_to_civ, replaces
}
Unit instance { id, type, owner, plot, hp (0..100), moves_left, xp, level,
                promotions[], formation (single|corps|army), charges, fortify_turns,
                embarked, activity (awake|sleep|fortify|alert|heal|skip|automated) }
```

## Stacking (1UPT)
- Per tile, at most one unit per **layer**: military (land or naval combat unit), civilian (Settler, Builder, Trader, Great Person, Archaeologist, Naturalist, religious units), support (Battering Ram, Siege Tower, Medic, Military Engineer, Observation Balloon, Anti-Air Gun, Mobile SAM, Supply Convoy, Drone). Air units are based in cities/airstrips/carriers and do not occupy map layers.
- Units of different owners never share a tile. A civilian on a tile with a friendly military unit is "escorted"; units can be **linked** to move together.
- Moving into a tile with an enemy civilian alone captures it (a captured Settler becomes a Builder for the captor; a Builder is captured as a Builder; Traders are plundered/destroyed). Religious units and Great People cannot be captured or attacked by military units.

## Unit classes and representative units
(CS = melee combat strength, RS = ranged strength, BS = bombard strength, M = movement. Values are the shipped game's defaults; keep in data.)

| Class | Units by era (CS / RS) |
|---|---|
| Recon | Scout (10, M3), Skirmisher [GS] (20/30), Ranger (45/60), Spec Ops (65/75) |
| Melee | Warrior (20), Swordsman (35, Iron), Man-at-Arms [GS] (45, Iron), Musketman (55, Niter), Line Infantry (65), Infantry (70), Mechanized Infantry (85) |
| Anti-cavalry (+10 vs cavalry) | Spearman (25), Pikeman (41), Pike and Shot [GS] (55), AT Crew (70), Modern AT (80) |
| Ranged | Slinger (5/15, range 1), Archer (15/25, range 2), Crossbowman (30/40), Field Cannon (50/60), Machine Gun (60/70) |
| Light cavalry (high mobility; subject to ZOC like other land units) | Horseman (36, M4), Courser [GS] (44, M5), Cavalry (62, M5), Helicopter (82, M4, ignores terrain) |
| Heavy cavalry | Heavy Chariot (28, M2; +1 movement if it starts its turn on flat terrain without features [GS]), Knight (48), Cuirassier [GS] (64), Tank (80), Modern Armor (90) |
| Siege (bombard: full damage to districts/walls; cannot move and attack in the same turn unless it has the Expert Crew promotion) | Catapult (23 / BS 35), Trebuchet (35/45), Bombard (43/55), Artillery (60/70), Rocket Artillery (70/80, range 3) |
| Naval melee | Galley (25, coast only), Caravel (55), Ironclad (60, Coal), Destroyer (70) |
| Naval ranged | Quadrireme (20/25), Frigate (45/55, Niter), Battleship (60/70, Coal/Oil), Missile Cruiser (70/85) |
| Naval raider (invisible until adjacent; coastal raid) | Privateer (40/50), Submarine (65/75), Nuclear Submarine (80/85) |
| Naval carrier | Aircraft Carrier (65; holds 2–3 air units, more with promotions) |
| Air fighter (air superiority, patrol, intercept) | Biplane (80), Fighter (100), Jet Fighter (110) |
| Air bomber (bombard districts/units) | Bomber (85 BS), Jet Bomber (95 BS) |
| Support | Battering Ram (melee attacks vs Ancient walls deal full damage), Siege Tower (melee bypass walls), Military Engineer, Medic (+20 heal adjacent), Observation Balloon / Drone (+1 range to siege), Anti-Air Gun / Mobile SAM, Supply Convoy |
| Giant Death Robot | 130 CS, RS 130, Uranium; late |
| Civilians | Settler, Builder, Trader, Archaeologist, Naturalist, Great People, Missionary, Apostle, Inquisitor, Guru, Spy (handled as non-map unit while on mission) |

Unique units replace generic ones (e.g., Roman Legion builds roads/forts, Greek Hoplite +10 next to another Hoplite, Egyptian Maryannu Chariot Archer, Scythian Saka Horse Archer and double light cav production, English Redcoat, Zulu Impi, Mongol Keshig, Japanese Samurai, Aztec Eagle Warrior captures units as Builders, American P-51, Indian Varu, Sumerian War-Cart, Norwegian Berserker, Chinese Crouching Tiger, Spanish Conquistador, Brazilian Minas Geraes, Kongo Ngao Mbeba, French Garde Impériale, German U-Boat, Russian Cossack, Arabian Mamluk).

## Movement
- Each unit has Movement Points (MP). Entering a tile costs the terrain + feature cost (flat 1, hills/woods/rainforest/marsh 2, combined up to 3). Roads override cost.
- A unit may always move into an adjacent tile if it has full MP even if the cost exceeds its MP; otherwise it needs enough remaining MP.
- River crossing (edge) ends movement unless bridged road.
- **Zone of Control (ZOC)**: every military land unit, encampment, and city center exerts ZOC on adjacent tiles. A unit that *enters* a tile in enemy ZOC loses all remaining MP. Only units with an explicit "ignore ZOC" ability (granted by specific promotions or unique-unit abilities; data) are exempt. Naval units exert ZOC on adjacent water.
- **Borders**: units cannot enter another major civ's territory without Open Borders, Alliance, or war. Borders are open to everyone until the territory owner completes the Early Empire civic. Exempt at all times: religious units, Traders, and Great People; Archaeologists need Open Borders; Spies travel off-map. City-states: always allowed unless at war.
- **Embarkation**: land units can embark onto coast (Shipbuilding) and ocean (Cartography). Embarking/disembarking costs all MP (unless promotions/abilities). Embarked movement: 2 base, raised by later techs (+1 Mathematics, +1 Steam Power; tunable). Embarked units have weak defense (≈ their era's naval base, very vulnerable) and cannot attack (except some abilities).
- Pathfinding: A* with per-unit cost function; respects ZOC, borders, embark rules, visibility (known tiles only), and remaining-turn split.
- **Teleport/airlift**: Airport enables airlifting a land unit between cities with Airports; Rapid Deployment civic.
- Units with 0 MP can still fortify/sleep.

## Combat resolution
All damage values are integers; units have **100 HP**. Cities and walls have their own HP.

### Strength calculation
```
effective_CS(unit, context) =
    base_strength(for this attack type)
  + promotion bonuses
  + terrain defense (defender only: hills +3, woods/rainforest +3, marsh/floodplains −2, river-crossing attacker −5)
  + fortification (+4 after 1 turn fortified, +6 after 2 turns; ranged/siege get fort bonus too) [tunable]
  + flanking: +2 per other friendly military unit adjacent to the defender (attacker only; requires Military Tradition)
  + support: +2 per friendly military unit adjacent to the defender (defender only; requires Military Tradition)
  + great general/admiral aura (+5 within 2 tiles, same or earlier era)
  + formation (+10 Corps/Fleet, +17 Army/Armada)
  + class bonuses (anti-cav +10 vs cavalry; ranged −17 vs districts/cities; walls penalties)
  + difficulty bonus (AI), civ/leader abilities, policies, religious beliefs (Crusade +10 near foreign cities with your religion), Golden/Heroic age effects
  − health penalty: −1 per 10 HP missing (i.e., −floor((100 − hp)/10))
  − resource shortage penalty [GS]
```

### Damage formula
```
damage_to_target = round(30 × exp(0.04 × (S_attacker − S_target)) × rand(0.8, 1.2))
```
- **Melee**: both sides take damage. Attacker deals damage to defender using `S_att − S_def`; defender deals counter damage to attacker using `S_def − S_att` (each with its own random roll). If defender dies and attacker survives, attacker moves into the tile (except some cases: attacking from embarked, cities need HP 0).
- **Ranged** (ranged units, cities, encampments, naval ranged, air): only the target takes damage. Attacker uses RS; defender always uses its melee CS (defense strength), never its RS. Range measured in tiles; requires line of sight (unless indirect fire via Observation Balloon/Drone for siege). Ranged attack against districts/cities: −17 (siege units exempt).
- **Bombard**: siege/bomber attack strength; full damage vs districts and walls.
- **Walls**: while a city has wall HP > 0, attacks hit wall HP first: non-siege melee and ranged units deal 50% damage to walls (tunable) and cannot reduce city HP until walls fall; siege units and Battering Ram-supported melee deal full damage to walls. Siege Tower lets melee hit city HP directly through Ancient/Medieval walls.
- Kill: HP ≤ 0 → unit destroyed. Corps/Army destroyed as a whole.
- Air combat: fighters on patrol intercept bombers within range; Anti-air units in range (and AA in cities with Renaissance walls/later) deal damage to attacking aircraft.

### XP and promotions
- XP gain per combat: attacker 5 (melee vs unit), ranged attacker 3, defender 4, no extra XP for a kill (tunable); combat vs barbarians cannot take a unit beyond its first promotion. Barracks/Stable, Armory and Military Academy each give +25% XP to land units trained in that city.
- Promotion thresholds: XP needed for each next level is 15 × level (15, 30, 45, 60, 75, ...); XP resets to 0 on promotion. Promotion choice is a tree per promotion class (two branches, 7 promotions, tiered). Taking a promotion heals the unit 50 HP and ends its turn (tunable flag).
- Example promotion tree (Melee): Battlecry (+7 vs melee/ranged) / Tortoise (+10 vs ranged) → Commando (scale cliffs, +1 move) / Amphibious (no river/embark penalty) → Zweihander (+7 vs anti-cav) → Urban Warfare (+10 fighting in districts) / Elite Guard (+1 additional attack per turn).
- Each class has its own tree: Recon, Melee, Ranged, Anti-Cav, Light Cav, Heavy Cav, Siege, Naval Melee, Naval Ranged, Naval Raider, Naval Carrier, Air Fighter, Air Bomber, Giant Death Robot. Support units have no promotions.

### Healing (unit must not move/attack this turn; "heal" or skip)
- Per turn: enemy territory 5 HP, neutral 10, friendly 15, in a city or Encampment 20 (tunable); Medic +20 to adjacent units; Promotions (e.g., March) heal after moving. Naval units heal only in friendly territory (unless promotion). Air units heal at bases.
- [GS] Without required strategic maintenance: no healing.

### Formations
- **Corps/Fleet** (Nationalism): merge 2 same-type units (+10 CS). **Army/Armada** (Mobilization): merge 3 (+17 CS). Military Academy trains them directly (Corps at 175% and Army at 250% of base cost; tunable).
- **Escort formation**: link civilian + military to move together.

### Upgrades
Pay gold (+ strategic resources in GS) to upgrade to the next unit in the line when tech is known, unit is anywhere in its owner's territory and has movement left; keeps promotions; takes the turn.

### Great Generals and Admirals
- Earned via GPP (see 07). Passive aura: +5 CS and +1 movement to land units within 2 tiles that started their turn in range; applies only to units of the GG's era or earlier (Great Admirals: same for naval units).
- Retire ability (one-time): e.g., create a Corps, grant XP, build a Fort, +loyalty, combat bonuses; each historical GG has a unique ability (data).

### War support systems
- **Pillage**: military unit spends MP (3) to pillage improvement/district; gains yields per type (e.g., farms → food/heal; mines → gold; campus → science; holy site → faith). Pillaged improvements produce nothing until repaired.
- **Coastal raid**: naval melee/raider pillages coastal improvements.
- **Plunder trade route**: military unit on trader's path captures route → gold.
- **Capture civilians**: settlers/builders become yours.

## Nuclear weapons
- Manhattan Project (Nuclear Fission) → can build Nuclear Device (Uranium). Operation Ivy → Thermonuclear Device.
- Delivery: from Missile Silo, Bomber/Jet Bomber, Nuclear Submarine. Missile Silo range is 12 tiles (tunable).
- Effects: Nuclear Device radius 1 (Thermonuclear radius 2): destroys most units, pillages/damages districts, reduces city pop (Nuclear Device −4, Thermonuclear Device −8; tunable), contaminates tiles with **fallout** for 10 turns (units entering take damage), heavy grievances and diplomatic penalties from all civs; triggers emergency [GS]. Can't target outside war.

## Barbarian AI combat
Same rules; barbarians don't gain promotions beyond spawn and target weakest nearby city/civilian.
