# Record: the cities spec audited again against the core

Status: done, 2026-10-10. Previous: `2026-10-09-ai-audit.md`. After the AI audit, a second read of specs/civ6/02-cities.md (the first is `2026-10-09-city-rules.md`) against the core found 3 rules off. Chosen by Claude under James's standing consent.

## Fixes

- **A unit bought with Faith needs its strategic resource** (02: Purchasing, "[GS] Units that need a strategic resource also need the resource cost in the stockpile when trained or bought"). Training and Gold purchases already checked and spent it; a Faith purchase (Theocracy, the Grand Master's Chapel, Monumentality, Lahore) did neither, so a Theocracy could buy Swordsmen with no Iron. The purchase is now refused with `NotEnoughResources` and spends the resource like a Gold purchase (`strategicCostIn`, so the Black Marketeer's discount applies).
- **A pillaged improvement stops working** (02: Pillaging; 05: "Pillaged tiles produce nothing until repaired"). Its yields already stopped, but its resource still counted as improved (luxury amenities, strategic accumulation, resource counts in `Game::resourceImproved`) and its Housing still counted (`Game::improvementHousing`). Both now wait for the repair.
- **Mohenjo-Daro and the Aqueduct.** Its suzerain's cities house as if on a river (city-state data), and `cityReport` counted that, but `Game::aqueductHousing` still saw a dry city and topped it up to 6 on top of the river's 5. The Aqueduct now adds the fresh-water +2 there.

## Results

- 128 AI games (Small, 6 AI, turn 200) against #317: science -1.1 ± 0.6, population -0.3 ± 0.2, the rest within one standard error (inferred: the rules are stricter, mostly pillaged Farms housing nobody until repaired).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.

## Tests

- `a_unit_bought_with_faith_needs_and_spends_its_strategic_resource` (test_rules_gaps.cpp), `a_pillaged_improvement_gives_no_resource_or_housing` (test_improvements.cpp), and an Aqueduct case in `suzerain_bonuses_in_code` (test_citystates.cpp). All three fail on the old code.
- Mutation check: 5 mutants (each fix undone, the spend dropped); the tests catch all 5.
