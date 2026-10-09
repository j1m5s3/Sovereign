# Record: district, building and wonder rules the spec audit found off (03)

Status: done, 2026-10-09 (four parts, #253 to #256). Previous: `2026-10-09-map-audit.md`. A read-only audit compared specs/civ6/03-districts-buildings-wonders.md with the core, leaving out what `2026-10-06-spec-audit-gaps.md`, `2026-10-06-spec-audit-part-2.md`, `2026-10-09-city-rules.md` and the decisions already settle, and confirmed 11 rules that differ in code. Chosen by Claude under James's standing consent; one PR per part.

## Part 1: pillage, mountains, the Golden Gate and Government Plaza tiers

- **A pillaged district's buildings stop working everywhere** (03: Pillage, "a pillaged district or building stops working until repaired"). Their yields, Housing and Amenities already stopped; now so do their Great Works' yields and Tourism, great person points (the district's and its buildings'), trade route capacity, Power drawn and given (a Factory draws none, a power plant powers no city and burns nothing, a Hydroelectric Dam gives none), powered yields and Amenities, Magnus's and Reyna's power bonuses, civ and leader abilities on district buildings, and the buildings' modifiers. The pillaged district itself gives no Housing or Amenities, and a pillaged Dam no longer stops floods (`buildingIdle` in state.h, used across `Game::cityReport`, `greatPersonPointsPerTurn`, `burnPower`, `renewablePower`, `tradeRouteCapacity`, `tourismBase` and the modifier holders; `districtHousing`, `districtAmenities`, `cityPrevents`).
- **Nothing is built on a tunnelled Mountain** (03: districts "cannot go on a Mountain"): a Mountain Tunnel made the plot land-passable, which also let districts and land wonders on it. Only a wonder built on a Mountain (Machu Picchu) goes there (`Game::canPlaceDistrict`, `Game::wonderFits`).
- **A strategic resource hidden under a district or wonder is granted once revealed** (03: "a strategic resource that is still hidden does not block placement (it is granted once revealed)"): the plot counts as improving it, so its owner gathers it and it counts wherever an improved resource does (`Game::resourceImproved`).
- **The Golden Gate Bridge spans two opposite land plots** (03: "coast tile spanning two opposite land tiles"); before, any land beside the coast plot would do (`Game::wonderFits`).
- **Government Plaza buildings need a government of their tier** (03: "each tier requires a government of that tier"; Sovereign reading: or a higher tier). A building's tier comes from its prerequisites at rules load (`BuildingType::plazaTier`: 1 with none, else one more than theirs), so a Chiefdom builds none, a tier 1 government the first tier, and so on (`Game::canProduce`).

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: no clear change (science +0.7 ± 0.6, population +0.3 ± 0.2, Gold -5.0 ± 3.6). A 150-turn 6-AI game takes the same time.
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 play out differently.
- Tests: `a_pillaged_district_and_its_buildings_stop_working` (each district pillaged against it left out: Housing, Amenities, culture, trade capacity, Tourism, loyalty, the Zoo's modifier, flood protection, great person points), `a_pillaged_districts_buildings_lose_their_bonuses` (powered yields and Amenities, Hypatia and James Watt, Magnus, Charlemagne and Japan), `pillaged_districts_draw_and_give_no_power`, `a_tunnelled_mountain_holds_no_district_and_only_a_mountain_wonder`, `a_hidden_strategic_resource_under_a_district_or_wonder_is_granted_once_revealed`, `the_golden_gate_bridge_spans_opposite_land`, `government_plaza_buildings_need_a_government_of_their_tier`; `exclusive_buildings_and_either_prerequisite` now runs under Democracy. Mutation check: 28 mutants, all caught (the second pillage test was added to catch the last 6).

## Part 2: yields by Appeal and Farms on hills

- **The Preserve's Grove and Sanctuary feed its unimproved neighbours by their Appeal** (03: Preserve; data: `Adjacent_AppealYieldChanges`). The Grove gives +1 Food and +1 Faith on a Charming plot, and +2 Food, +2 Faith and +2 Culture on a Breathtaking one. The Sanctuary gives +1 Science and +1 Gold, or +2 Science, +2 Gold and +2 Production. The data table names only the Preserve, so `tools/rules_gen/gen_rules.py` gives each row to the building whose yields they are (source: https://primagames.com/?p=314685) as `appealYields`. A plot beside two Preserves with a Grove gains the Grove's yields once (Sovereign reading). A water plot gains nothing unless it is a natural wonder. Nothing is gained beside a pillaged or unfinished Preserve (`Game::preserveYields`, from `Game::plotYields`). Before, both buildings did nothing.
- **A Seaside Resort yields Gold equal to its plot's Appeal** (03), as well as Tourism. The extract leaves out the data's `YieldFromAppeal`, so the generator sets `appealYield` (`Game::improvementYields`).
- **A Farm goes on Grassland or Plains Hills only with Civil Engineering** (03). The extract leaves out the data's `PrereqCivic`, so the generator sets the Farm's `terrainUnlocks` (`Game::improvementFits`). A Farm for the plot's resource (Wheat on Plains Hills) needs no civic.

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 1: AI Builders mine the hills they used to farm before Civil Engineering, so production rises 182.5 to 186.2 (+3.8 ± 1.5) and population falls 66.9 to 65.6 (-1.3 ± 0.4); science -2.0 ± 1.2 and culture -1.2 ± 0.8 are within noise, Gold +9 ± 7. A 150-turn 6-AI game takes the same time.
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 play out differently.
- Tests: `the_preserves_buildings_feed_its_neighbours_by_appeal` (both bands, both buildings, a lake, a Farm, a pillaged and an unfinished Preserve, a second city's Preserve), `a_seaside_resort_yields_its_appeal_in_gold`, `a_farm_on_hills_needs_civil_engineering`; two hill Farms in older tests now come with Civil Engineering. Mutation check: 11 mutants, all caught (the pillaged Preserve case got two more Woods to catch the last: a pillaged district lowers the plot's Appeal by 2, so the plot was no longer Charming).

## Part 3: several Neighborhoods and Canals, and the Panama Canal

- **A city may hold several Neighborhoods and Canals** (03: placement flags come from data; the Neighborhood and the Canal have no `OnePerCity`). `tools/rules_gen/gen_rules.py` marks a district with neither "one per city" nor "one per river" as `repeatable`. A city places one at a time: while one is unfinished, producing that type resumes it (production progress stays keyed by type), and once it is done another may go on a free plot (`districtInWork` in state.h, used by `Game::canProduce`, `Game::buildableItems`, `Game::districtOpenIn`, `Game::districtPurchaseCost`, the production commands, the AI and the random bot). Finishing one leaves the earlier ones alone, so moments and culture bombs count once each (`Game::completeItem`). A building of the type's district (the Food Market) sits in the first finished one.
- **The Dam stays one per city** (Sovereign reading). The data allows one per river; rivers carry no identity in the core, so a second Dam on the same river could not be told apart.
- **The Panama Canal is placed and sailed as a canal** (03: "as a canal"): on flat land linking two bodies of water, or water and the City Center, like the Canal district (`Game::canalLinks`, `WonderPlacement::canal`); once built, ships sail through its plot (`Game::stepInto`); +10 Gold. Sovereign reading: it covers one plot; the data's multi-plot span is not modelled. Before, `tools/rules_gen/gen_rules.py` left it out; all 53 world wonders are now in the rules.
- Follow-up for the Unreal front end: its production chooser places a new district only when the city has none of the type (`SovPlayerController.cpp`, `City->district(Item.type, false)`), so a second Neighborhood or Canal needs `sov::districtInWork` there before a human can place one.

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 2: no change worth the name (population -0.01, Gold +0.2 ± 0.4); the AI seldom fills a city's Neighborhood before turn 200 and values a canal low. A 150-turn 6-AI game takes the same time.
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; 4 play out differently.
- Tests: `a_city_holds_several_neighborhoods_and_canals` (a second of each placed, resumed after switching away, offered only with a free plot, bought by a Contractor, its Housing counted, a second Canal's moment counted once, a Campus still one per city), `the_panama_canal_links_water_and_lets_ships_through` (both links, dry land, hills, finished and unfinished); `wonder_rules_data` counts 53 wonders. Mutation check: 16 mutants, all caught (a queued second Canal on dry land was added to catch the last).

## Part 4: the district discount

The discount for a district type built less than the player's own average (03: District cost, `COST_PROGRESSION_NUM_UNDER_AVG_PLUS_TECH`) now follows the spec in three ways (`Game::districtCost`):
- **A counts every specialty district type unlocked**, those under the population limit, so the Preserve too; before, only the types with that cost model counted, which left the Preserve out.
- **B is the count of specialty districts finished as of the player's last tech or civic** (a Future Tech or Future Civic too), kept in `Player::districtsCounted` (save version 88) and set by `Game::completeNode`. Before, a district counted the moment it was finished.
- **A type's count takes in districts placed but unfinished.** The district a city has placed and is building does not count against itself, so its cost is the one it was offered (`productionCost` and district purchase pass the city; Sovereign reading, as Civ VI fixes a district's cost when it is placed and the core does not). The AI prices districts the same way.
- Follow-up for the Unreal front end: its production chooser shows `districtCost(Me(), Item.type)`, the price of one more; for a district the city has already placed it should pass the city.

Results:
- 128 AI games (Small, 6 AI, turn 200) against part 3: no clear change (population +0.2 ± 0.2, science +0.3 ± 0.6, Gold -5.1 ± 4.6). A 150-turn 6-AI game takes the same time.
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 play out differently.
- Tests: `the_district_discount_counts_placed_districts_and_waits_for_research` (no discount until a tech is done, then the discount; the count saved and loaded; the Preserve's type in A; an unfinished Holy Site elsewhere counts, its own city's does not; production and purchase cost), and `district_cost_grows_with_progress_and_discounts` sets the count. Mutation check: 10 mutants, all caught (the save round trip was added to catch the last).

## Unsure, not changed

- Whether a regional building's range is measured from its district (the core measures from the City Center).
- Whether unfinished districts count toward adjacency and Appeal.
- Whether a city can add a second worship building after its religion changes.
