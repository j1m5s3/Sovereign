# Record: city focus (02: Citizens)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-ransom.md`. The cities audit (`2026-10-09-city-rules.md`) left city focus open: the spec has citizens "assigned by city focus (balanced default, or Food/Production/Gold/Science/Culture/Faith emphasis)", and the core had one fixed weighting. Chosen by Claude under James's standing consent.

## Rules

- **`City::focus`** (`CityFocus`: Balanced, Food, Production, Gold, Science, Culture, Faith), set by the new `SetCityFocus` command (`Command::setCityFocus`); the city's citizens are reassigned at once. Locked plots stay locked.
- **Weights** (`citizenWeights` in `city.cpp`): balanced stays as it was (Food 4, Production 3, Gold, Science and Culture 2, Faith 1); a focus raises its yield's weight to 8, for plots and specialist slots alike. The spec gives no numbers; this is a Sovereign reading.
- **Saves:** version 87; a save naming no focus is refused.
- The AI keeps the balanced focus, so AI games play out as before.

## Results

- 8 long AI games in 8 setups (up to Huge, 400 turns) end with the same cities, units and command counts as before the change; only the state hashes differ, because the save holds the new field.

## Tests

- `city_focus_steers_its_citizens`: one citizen among grassland, a wooded plains plot and four specialist slots: balanced works the woods, Food takes grassland, Science, Gold, Culture and Faith take their own district's slot, Production the woods; out-of-range focuses are refused; the focus is saved; a save with a bad focus does not load.
- Mutation check: 13 mutants (the weight, the focus-to-yield table, plots and specialists each scored without the focus, the command not stored, not reassigning, each validation bound, the save field and its check, the command not applied); the tests catch all 13 (the test now stops at a wrong citizen count rather than read an empty worked list, which crashed when the command was not applied).

## Left open

- **Unreal:** the city panel needs a focus control that submits `Command::setCityFocus` (a PC session task; the cloud session does not edit Unreal code).
- **Found on the way:** three places name the Theater Square `DISTRICT_THEATER` while the rules call it `DISTRICT_THEATER_SQUARE` (`city.cpp` Grand Opera and the Pen, Brush and Voice dedication, a promotion in `promotions.json`), so those effects never fire. The next PR.
