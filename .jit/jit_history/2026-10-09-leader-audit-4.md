# Record: leader character rules the audit found off (part 4)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-3.md` (body doubles). Two more of the doc's section 3 rules. Chosen by Claude under James's standing consent.

## Change

- **XP for the ruler's first visit to each of the civ's city centers and districts** (doc section 3, "first visit to each of your districts and cities"): `LEADER_XP_FIRST_VISIT` (3, Sovereign tuning) when the ruler enters such a plot for the first time (`Game::leaderVisit`, from `Game::enterPlot`). The plots visited are kept per civ, not per reign, in `Player::leaderVisits` (save version 93), so a successor does not earn them again. Another civ's cities and districts earn nothing; a wonder's plot or the city's other land is not a district.
- **Statesman promotions improve citizen interactions** (doc section 3, "better outcomes from citizen interactions"): Wary and Spymaster each carry `STANCE_POWER` 25 (`UnitEffectKind::StancePower`). Each makes a Benevolence last 25% longer (10 turns, 12 with one, 15 with both) and a Fear give 25% more loyalty (20, 25, 30). Sovereign tuning; the cost and the downsides are unchanged.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 3): every game identical. Inferred, not traced: AI rulers rarely walk into their own districts by then, and its leader XP waits on promotions it does not take there.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from part 3. Time per game unchanged (two alternating pairs).
- Tests: `the_ruler_earns_xp_for_first_visits` (a city center and a Campus once each, not again, not the city's other land; kept in a save), `a_visit_to_a_foreign_district_earns_nothing`, `a_statesman_handles_citizens_better` (none, one and two Statesman promotions). Mutation: 9 mutants, all caught (the foreign-district one after its test was added).

## Still open from the audit

Bodyguards (named companions), Warlord's weapon-type bonus and leading a Corps or Army, in-person diplomacy, duel war score (see part 1).
