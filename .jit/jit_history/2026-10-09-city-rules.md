# Record: city rules the cities spec audit found off (rules gaps)

Status: done, 2026-10-09. Previous: `2026-10-09-start-guarantee.md`. A read-only audit compared specs/civ6/02-cities.md with the core and found 11 rules that differ, plus 3 loyalty rules from the leader doc that were never built. This record covers 9 of the 11; the rest are under "Left open". Chosen by Claude under James's standing consent.

## Fixes

- **Walls do not mend on their own** (02: City combat). The city's walls and its Encampment's no longer regain 10 HP a turn after 3 quiet turns; that was a stand-in from before projects existed (`2026-10-05-rules-core.md`). Repair Outer Defenses is offered only when the city's or its Encampment's walls are down and the city has gone 3 full turns without an attack (`COMBAT_HEAL_OUTER_DEFENSES_COOLDOWN`), and it mends both. The AI puts it at the front of a city's queue as soon as it is offered (it costs 1 Production; work on the item it replaces is kept).
- **City center yields** (02: Founding): the plot is raised to 2 Food and 1 Production before its resource adds to it, so plains with Wheat give 3 Food, not 2.
- **No city on an Oasis or a Mountain** (02: Founding), not even one a Mountain Tunnel runs through. `canHoldCity` (mapgen.h) is used for founding, start plots and city-state plots; `FeatureType::noCity` marks oases and natural wonders.
- **Lighthouse:** +2 Housing more when the city center lies beside Coast or a lake (02: Housing). This is a new `CITY_IS_COASTAL` requirement and a hand-written modifier.
- **Preserve [GS]:** a major civ's finished Preserve claims the unowned plots around it, a culture bomb (02: Border growth).
- **Border growth** counts −105 for a natural wonder and −5 for an improvement (`PLOT_INFLUENCE_NW_COST`, `PLOT_INFLUENCE_IMPROVEMENT_COST`), as the spec lists.
- **Unrest and Revolt stop growth** whatever growth bonuses the city has. Before, Hanging Gardens or Migration Treaty A still let such a city grow at 15–35%.
- **A pillaged district adds no city strength** (02: City combat, "non-pillaged").
- **Naval melee units take coastal cities** (02: City combat, a Sovereign reading of "melee" as in Civ VI; the spec line now says so).

## Results

- 128 AI games (Small, 6 AI, turn 200) against #242: population +1.5 ± 0.5, science +3.0 ± 1.4, production +4.9 ± 1.8, culture +2.1 ± 0.9, gold +7.3 ± 5.0 (inferred: mostly the center floor and the Lighthouse's Housing).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; 5 setups saved and reloaded at 8 cuts end on the straight game's state.
- The 30-turn golden game's state hash is unchanged; the rules checksum changes (a new modifier).

## Tests

- `cities_heal_and_walls_wait_for_the_repair_project` (replaces `cities_heal_and_walls_repair_after_a_quiet_spell`), `ai_repairs_its_walls`, `city_center_floor_comes_before_its_resource`, `city_not_founded_on_an_oasis_or_a_mountain` (oasis, tunnelled mountain and natural wonder; start plots and city-state plots), `city_lighthouse_housing_by_the_coast`, `city_borders_reach_for_wonders_and_improvements`, `city_in_unrest_does_not_grow`, `city_preserve_claims_the_plots_around_it` (a city-state's does not), `a_ship_takes_a_beaten_coastal_city`, `a_pillaged_district_adds_no_city_strength`.
- Mutation check: 21 mutants of the new code (each condition and threshold broken, each fix undone one at a time, the Lighthouse modifier removed); the tests catch all 21 (the natural wonder founding case and the city-state Preserve were added to catch the last two).

## Left open

- **City focus** (02: citizens assigned by a city focus, balanced or a Food, Production, Gold, Science, Culture or Faith emphasis): the core has one fixed weighting and tile locks. It needs a command, a saved field and an Unreal control; a later PR.
- **Shrine and Temple for Faith** (02: Purchasing says Holy Site buildings are always bought with Faith; the generated buildings table says Gold only). The two sources disagree, so it stays as the data says until checked.
- **Leader loyalty** (leader doc): the leader's aura, the Statesman branch's loyalty pressure and the loyalty drop when the leader is lost; done next, in `2026-10-09-leader-loyalty.md`.
