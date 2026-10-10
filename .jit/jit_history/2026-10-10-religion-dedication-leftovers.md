# Record: religion and dedication rules left open by the second-pass audits

Status: done, 2026-10-10. Previous: `2026-10-10-tech-civics-audit-4.md`. Four rules the religion and climate audits (`2026-10-10-religion-audit-2.md`, `2026-10-10-climate-audit-2.md`) and the dedications slice (`2026-10-05-dedications.md`) left for later. Chosen by Claude under James's standing consent.

## Fixes

- **Holy Waters heals beside a rival's Holy Site** (06: "Holy Sites of cities following your religion"): +10 beside the Holy Site of any city whose majority religion is the healing player's own religion. Before, only the player's own cities counted, and they counted whenever their majority religion held the belief, even a rival's religion. God of Healing stays with the player's own Holy Sites.
- **Dar-e Mehr: +1 Faith for each era since it was built or its Holy Site was last repaired** (06), by the world era. `City::worshipEra` is set when the building is finished or bought, and again when a pillaged Holy Site's repair ends. Save version 102.
- **Sky and Stars in a Golden Age** (09; data: eras-moments-loyalty): +2 Aluminum a source in the player's own cities, and the Eurekas of Satellites, Nuclear Fusion and Nanotechnology while the world is in the Information era. The Future era's three Eureka techs do not exist in these rules.
- **Automaton Warfare in a Golden Age** (09; same data row): +1 Uranium a source in the player's own cities and +3 Uranium a turn.

## Left open

- Population loss on disaster plots other than City Centers (an unknown engine rule), and the Heavy-intensity disaster range.
- Per-work Great Work yields: the data has no rows for them.

## Results

- 128 AI games (Small, 6 AI, turn 200) against tech and civics audit 4: every figure the same within a hundredth (these rules rarely come into play by turn 200).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant (9) caught.
- The 30-turn golden game's state hash file regenerated (save layout).

## Tests

`holy_waters_heal_beside_a_rivals_holy_site_of_our_religion` and `a_dar_e_mehr_gains_faith_with_the_eras` (test_religion.cpp), `sky_and_stars_and_automaton_warfare_reward_a_golden_age` (test_eras.cpp).
