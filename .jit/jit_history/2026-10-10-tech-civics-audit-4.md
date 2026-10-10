# Record: the techs, civics and governments spec audited a fourth time against the core

Status: done, 2026-10-10. Previous: `2026-10-10-map-audit-2.md`. Another read of specs/civ6/04-tech-civics-government.md (earlier passes: `2026-10-09-tech-civics-audit.md`, `-2.md`, `-3.md`) against the core. Boosts, research and civic costs, era scores for boosts, government slot counts and legacy bonuses, policy changes and anarchy match apart from the fixes below. Chosen by Claude under James's standing consent.

## Fixes

- **World Ideology option B takes the government's own Wildcard slot** (World Congress), never a wonder's: before, the player lost their last slot, which under Big Ben or the Alhambra was the wonder's. `Game::wildcardsTaken` (0 or 1, none under a government with no Wildcard slot) shortens the government's run of slots in `policySlotType` and the slot count in `syncPolicySlots`.
- **No government change during an interregnum** (leader doc section 5: the ruler's death is an anarchy): `canAdoptGovernment` answers `ChangesLocked`, as policy changes already did.
- **Anarchy pays no monopoly Gold** (04: no yields in anarchy): `goldPerTurn` already dropped founder-belief Gold.
- **The research panel's output matches the turn**: `outputPerTurn` counted Faith in anarchy and left out founder-belief Science, Culture and Faith, which `processCities` adds.

## Left open

- The policy cost globals (`POLICY_COST_EXPONENT`, `POLICY_ENACTED_DISCOUNT`) are in the data but their formula is not in the spec (an unverified engine rule); policy changes stay free in a change window and Gold-priced outside it as before.

## Results

- 128 AI games (Small, 6 AI, turn 200) against the map audit: Science +0.8 and Gold +2.9 a turn, Culture +0.3 (the AI reads `outputPerTurn` when it plans); everything else within one and a half standard errors.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant (7) caught.
- The 30-turn golden game's state hash is unchanged.

## Tests

`world_ideology_b_takes_the_governments_wildcard` (test_worldcongress.cpp), `no_government_change_in_an_interregnum` (test_research.cpp), anarchy in `a_monopoly_pays_gold_and_tourism` (test_monopolies.cpp), the panel's founder Science and anarchy in `sacred_places_pays_for_wonders_in_following_cities` (test_religion.cpp).
