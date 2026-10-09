# Record: Volcanoes and Geothermal Fissures on generated maps

Status: done, 2026-10-09. Previous: `2026-10-09-districts-audit.md`. Left open by the map audit (`2026-10-09-map-audit.md`): map generation placed neither feature, so the three generic eruptions never found a site, and the Geothermal Plant, the Campus and Aqueduct bonuses beside a fissure and the Goddess of Fire's fissure Faith never applied. Chosen by Claude under James's standing consent.

## Change

- **Generated maps now carry Volcanoes and Geothermal Fissures** (01: features; 09: eruptions). The spec gives no density, so this is a Sovereign reading, tuned in `data/rules/setup.json`: one Volcano per `MAPGEN_MOUNTAINS_PER_VOLCANO` (12) Mountains and one Geothermal Fissure per `MAPGEN_LAND_PER_FISSURE` (150) land plots, each on a plot its feature allows that has no other feature, and at least 4 plots from another of its kind (`generateMap`, step 4b, before resources). A Standard map gets about 9 Volcanoes and 10 Fissures. Which Volcanoes are active still comes from the disaster setting (`activeVolcanoes`).

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: the maps change, and the fissures' Science shows: science 134.5 to 137.8 (+3.3 ± 1.8), Gold +10.5 ± 8.3, population -0.6 ± 0.6, production -2.3 ± 2.4. Eight 150-turn 6-AI games take the same time (11.0 s against 11.2 s).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 play out differently.
- Three 250-turn 8-AI Standard games at Moderate disasters: 9 Volcanoes and 10 Fissures each; two of the three ended with Volcanic Soil from an eruption.
- Tests: `maps_have_volcanoes_and_geothermal_fissures` (counts, terrains and spacing on four Duel and two Standard maps). Mutation check: 4 mutants, all caught (two Standard maps were added to catch the spacing one).
