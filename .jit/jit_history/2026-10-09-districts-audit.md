# Record: district, building and wonder rules the spec audit found off (03)

Status: in progress, 2026-10-09 (part 1 done; parts 2 to 4 planned). Previous: `2026-10-09-map-audit.md`. A read-only audit compared specs/civ6/03-districts-buildings-wonders.md with the core, leaving out what `2026-10-06-spec-audit-gaps.md`, `2026-10-06-spec-audit-part-2.md`, `2026-10-09-city-rules.md` and the decisions already settle, and confirmed 11 rules that differ in code. Chosen by Claude under James's standing consent; one PR per part.

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

## Planned

- **Part 2: yields and terrain.** The Preserve's Grove and Sanctuary give yields by Appeal; the Seaside Resort's Gold equals the plot's Appeal; a Farm on Grassland or Plains Hills needs Civil Engineering; the Panama Canal wonder (needs `tools/rules_gen/gen_rules.py` to place it).
- **Part 3: repeatable districts.** A city may hold more than one Neighborhood, Canal or Dam; production progress is keyed by district type today, so this changes how a placed district is tracked.
- **Part 4: the district discount** (03: District cost, `COST_PROGRESSION_NUM_UNDER_AVG_PLUS_TECH`): count placed districts, keep B (completed districts) as of the last tech or civic finished, and count the Preserve in A.

## Unsure, not changed

- Whether a regional building's range is measured from its district (the core measures from the City Center).
- Whether unfinished districts count toward adjacency and Appeal.
- Whether a city can add a second worship building after its religion changes.
