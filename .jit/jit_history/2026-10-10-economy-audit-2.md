# Record: the economy and great people spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-10-cities-audit.md`. A second read of specs/civ6/07-economy-trade-great-people.md (the first is `2026-10-09-economy-audit.md`) against the core found 2 rules off that are fixed here, and 2 left open. Chosen by Claude under James's standing consent.

## Fixes

- **A Trader's range is 15 plots over land and 30 over water** (07: Range). Once a civ's Traders could embark, `Game::tradeWays` searched again with 30 plots of range for every plot, so land routes of 16 to 30 plots became legal. Range is now kept in units of `TRADE_ROUTE_BASE_RANGE` × `TRADE_ROUTE_WATER_RANGE_REFUEL`: a land plot spends the water range of them and a water plot the land range, so a land-only way still reaches 15 plots, a sea way 30, and a mixed way spends each leg's share. Refuelling (own cities, Trading Posts) fills it as before.
- **One Archaeologist per Archaeological Museum** (07: Archaeology; Unit_BuildingPrereqs). A city with a museum could train any number. `canProduce` now counts the civ's Archaeologists against its museums (the data carries no count, so the rule is in code by unit id).

## Results

- 128 AI games (Small, 6 AI, turn 200) against #318: gold -11.0 ± 4.3 (inferred: fewer long overland routes once Traders sail), the rest within about one standard error.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.

## Tests

- `a_traders_range_is_15_over_land_and_30_over_water` (test_trade.cpp), `one_archaeologist_per_archaeological_museum` (test_archaeology.cpp). Both fail on the old code.
- Mutation check: 6 mutants (each step cost made flat, the full range changed, the cap off by one, removed, or counting other civs' Archaeologists); the tests catch all 6.

## Left open

- **Per-work Great Work yields** (07: the 8 Writing works worth +4 Culture, the 9 Portrait and Landscape works worth 4 Tourism): every work of a type pays the type's base. The generated tables have counts but not which works, so `tools/civ6_extract` would need per-work rows from the game install first.
- **Sea routes need maritime access at both ends** (07: Range, from a forum source): an inland city's Trader may walk to the coast and sail. Low impact; not changed.
