# Record: each government gives its own bonuses (rules fix)

Status: done, 2026-10-09. Found by Claude while looking at the governments the AI picks.

## What was wrong

- Most governments' own bonuses (04: Governments; the data's "Inherent effects") did nothing. Only Autocracy's yields in the Palace city, Oligarchy's melee strength and combat XP and Fascism's war weariness had hand-written modifiers, and code carried Monarchy's envoys and Favor, Theocracy's land units for Faith and Synthetic Technocracy's three bonuses.
- Missing were: Autocracy's +1 to all yields in cities with a government building (Audience Chamber and the others, Consulate, Chancery) and +10% toward wonders; Classical Republic's +1 Amenity and +1 Housing in cities with a specialty district and +15% great person points; Monarchy's +1 Housing per level of walls; Theocracy's +5 Religious Strength, +0.5 Faith per citizen in cities with a governor and 15% off every Faith purchase; Merchant Republic's +10% Gold in cities with a governor and +15% toward districts; Fascism's +5 strength when attacking and +50% toward units; Communism's +0.6 Production per citizen in cities with a governor and +10% Science; Democracy's +4 Food and Production on routes to allies and suzerained city-states, 15% off Gold purchases and +1 alliance point a turn; Corporate Libertarianism's Production in cities with a Commercial Hub or Encampment, -10% Science and +1 a turn from each strategic resource; Digital Democracy's +2 Amenities, +2 Culture per district and -3 strength. The legacy cards that carry a government's bonus after leaving it already had theirs.

## Fix

- `gen_rules.py` reads each government's effects as it reads a policy card's (`policy_modifiers`) into `governments.json` (`modifiers`, and `untrackedEffects` for the lines it cannot read). New lines it reads: +N% wonder construction, district production, unit production and great people, +N% combat experience, +N Culture per district, and +N% Faith or Gold purchases. The hand-written government modifiers left `modifiers.json`; the generated ones cover them.
- `rules.cpp` loads them with source `Government`: they apply while that government is adopted, never in anarchy (`playerHasSource`). `indexModifiers` lists a government's city modifiers under it by effect (`Rules::governmentCityModifiers`), so a pass over a city's modifiers looks at those of its owner's government only, as it looks at the policies slotted only.
- A new effect, `ADJUST_PURCHASE_DISCOUNT_PERCENT` (player; `yield` GOLD or FAITH, which the loader checks), takes its percent off every Gold or Faith price of a unit, building or district: `purchasePrice`, the Faith prices of religious units and worship buildings, `districtPurchaseCost`. Faith purchases at a Gold price (Jesuit Education, Golden Pilgrimage) now take the Faith discount, not the Gold one. Theocracy's land units lost their own 85%: the discount gives it, now rounded down to a multiple of 5 like every price.
- In code: Democracy's alliance point (`processDiplomacy`) and Corporate Libertarianism's strategic resources (`accumulateStrategics`, sources in its own cities). Monarchy's Favor for Renaissance Walls left `favorPerTurn` for its generated modifier.
- Still in code as before: Monarchy's +50% envoys, Synthetic Technocracy's Power, projects and Tourism.

## Sovereign readings

- Classical Republic's +15% great person points multiply each city's points, as Pingala's Grants do, not the points policy cards give the player.
- The purchase discounts lower every price built on a purchase price: Levy Military and hiring from a barbarian camp included.
- Fascism's +50% is toward every unit, civilians too.

## Tests

- `governments_give_their_own_bonuses`: Autocracy (+2 Science with the Palace and an Audience Chamber, +10% toward the Pyramids, none in anarchy), Classical Republic, Monarchy (Housing for three levels of walls, Favor unchanged from before), Merchant Republic, Fascism, Oligarchy, Democracy's and Theocracy's discounts, Digital Democracy, Corporate Libertarianism. `democracy_adds_an_alliance_point_a_turn`, `rules_load_government_modifiers` (the effect loads from a government; a discount on Food is refused), `rules_index_city_modifiers_by_policy_and_government` (renamed: each government's city modifiers listed once, under it and their effect), `jesuit_education_buys_campus_buildings_with_faith` (new: the path had no test), and discount checks added to the worship building, Contractor and Golden Pilgrimage tests. On main 14 checks of the first test and the alliance test fail.
- Older tests changed: Theocracy's land unit price is in fives; under Theocracy the Grand Master's Chapel's Faith price takes the discount too.
- Mutants: 22 (each new line broken in turn); 18 fail a test, 2 fail the build, 2 survive: a suzerain's city-state sources would also earn Corporate Libertarianism's extra, and a multi-effect sum without its early return in anarchy sums the same (a government's modifiers lapse in anarchy anyway).
- The golden file's rules checksum changes; the game's state hash does not.

## Results

- 128 AI games (Small, 6 AI, turn 200) against main: no change past noise. Cities 8.4 to 8.3 (t -1.2), population 56.7 to 56.8, science 110.3 to 109.8 (t -0.4), culture 63.4 to 63.8, production 151.9 to 151.8, Gold held 194 to 188 (t -1.5). The AI still takes the highest tier on offer and keeps it.
- Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.
- Speed: the 150-turn benchmark games (Small, 6 AI) take about 5% more CPU over 8 seeds (4.9% and 5.7% in two runs). Profiles put about 0.7% in the new lookups (the purchase discounts, the governments' city modifiers, three more unit abilities to look through); the rest is path finding and sight in games that come out bigger: over 40 seeds at turn 150, 1.5% more cities (t +2.9) and 1.6% more units (t +1.1).
