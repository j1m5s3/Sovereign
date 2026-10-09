# Record: disaster rules the spec audit found off (09, part 1)

Status: done, 2026-10-09. Previous: `2026-10-09-diplomacy-audit.md`. A read-only audit compared specs/civ6/09-civs-eras-victory-climate.md with the core (civilizations, eras, victories, climate, disasters, power), leaving out what earlier records settle (`2026-10-05-civ-systems-2.md`, `2026-10-05-late-game.md`, `2026-10-06-spec-audit-part-2.md` and others), and confirmed 8 rules that differ in code. This first part fixes the 5 about natural disasters; the era countdown, unpowered Terrestrial Laser Stations and Wish You Were Here come in part 2. Chosen by Claude under James's standing consent.

## Change

- **A Dam or the Great Bath guards each of its city's plots from floods** (09: "Dams on the river (and the Great Bath wonder) prevent floods"; data: wonders, `PreventsFloods`). Before, a guarding Dam was checked only where a flood began, so a flood centered next door still swept the guarded city's floodplains, and the Great Bath prevented nothing (it gave only its Faith). Now a flood leaves out every plot of a city with a finished, unpillaged Dam or the Great Bath (`Game::cityPrevents`, `Game::strikeDisaster`); `tools/rules_gen/gen_rules.py` sets `preventsFloods` on the Great Bath.
- **A drought spares a city whose Aqueduct or Bath prevents it** (09: "Aqueducts and Baths prevent it"). Before, only the drought's own center was checked: one beginning next door pillaged the guarded city's improvements and took its Food. Now its plots keep both (`Game::plotYields`, `Game::strikeDisaster`).
- **A drought falls on Farms only** (09: "pillages farms"; data: `SPECIFIC_IMPROVEMENT_PILLAGED` and `_DESTROYED`). Before, the data's specific improvement was read as any improvement, so Mines, Pastures and Plantations went too. Now a Major Drought pillages Farms; a Withering Drought destroys 30% of them and pillages the rest (`DisasterDamageType::FarmDestroyed`, `FarmPillaged`).
- **Hurricanes wreck the coastal lowlands** (09: "both also pillage coastal lowland tiles"; data: Coastal lowland %). Before, the generator dropped the column. Now a Category 4 hurricane pillages every improvement (50% elsewhere) and 100% of districts (15% elsewhere) on coastal lowlands, a Category 5 all districts there (50% elsewhere) (`DisasterDamage::lowlandPercent`).
- **Storms and droughts keep their spacing** (09: "spaced at least 15 tiles apart"; data: Spacing, also 15 for droughts). Before, a storm could strike beside one still moving. Now a new storm strikes no nearer than 15 plots to a running storm, a new drought no nearer to a running drought (`DisasterType::spacing`, `Game::processClimate`).

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: all 128 play out differently, with no change past noise (Gold +8.4 ± 4.8, production +1.7 ± 1.2, population +0.4 ± 0.4, science +0.8 ± 1.0). Eight 150-turn 6-AI games take the same time (11.4 to 11.5 s against 11.4 to 11.6 s, three runs each).
- 8 long AI games in 8 setups (up to Huge, 400 turns, four with disasters at Heavy or Hyperreal): no crash or replay mismatch; all 8 play out differently.
- Tests: `floods_spare_the_plots_a_dam_or_the_great_bath_guards` (no guard, a Dam, the Great Bath), `a_drought_pillages_farms_and_spares_a_city_with_an_aqueduct` (a Farm and a Mine, a guarded neighbour's Farm and Food, a Withering Drought's destroyed and pillaged Farms), `storms_and_droughts_keep_their_spacing` (a tornado and a drought, with and without one running), `a_hurricane_pillages_every_lowland_improvement`. Mutation check: 15 mutants, all caught.

## Unsure, not changed

- Where a city's power reaches from (the core: its center).
- Reyna's Renewable Subsidizer: 09 says "+2 Power" per renewable, 08 and the data say +2 Gold; the core gives Gold.
