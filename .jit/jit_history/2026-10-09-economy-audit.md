# Record: economy rules the spec audit found off (07)

Status: done, 2026-10-09. Previous: `2026-10-09-religion-audit.md`. A read-only audit compared specs/civ6/07-economy-trade-great-people.md with the core (gold, trade routes, great people, Great Works, tourism, score), leaving out what earlier records settle (`2026-10-06-spec-audit-part-2.md`, `2026-10-05-tourism-sources.md`, `2026-10-05-theming.md`, `2026-10-05-archaeology.md` and others), and confirmed 3 rules that differ in code. Chosen by Claude under James's standing consent; one PR.

## Change

- **A trade route lasts whole round trips** (07: Duration, "the route ends when it gets back to the origin after the minimum duration has passed ... the smallest whole number of round trips (2 × path length each)"). Before, every route lasted the minimum (20 turns on Standard, plus the world era's increment, scaled by speed); `2026-10-06-spec-audit-part-2.md` had left this open. Now a route's turns are the fewest round trips, each twice its way's length in plots, that reach the minimum: a 3-plot way lasts 24 turns, a 10-plot way 20, an 11-plot way 22 (`Game::tradeRouteDuration`, set when the route starts). Sovereign reading: the spec says the trips "exceed" the minimum and that the route ends "after the minimum has passed"; a trip that divides the minimum ends with it. `Game::tradeRouteLength` stays the minimum.
- **Deeper debt disbands more units** (07: "disbands one unit; at −20 two units, and so on"; data: `GOLD_NEGATIVE_BALANCE_DISBAND_UNIT_LINE` −10, `_SUBSEQUENT_DISBAND_UNIT` −10). Before, one unit a turn went at any debt past −10. Now one goes at −10 and one more for each further 10 Gold, each turn, the costliest first (`Game::processCities`).
- **Inspirations count toward domestic tourists** (07: "lifetime culture generated, including culture from Inspirations"). Before, only the culture yield counted. Now an Inspiration's share of the civic's cost is added to `Player::lifetimeCulture` too; a Eureka adds nothing (`Game::grantBoost`).
- Follow-up for the Unreal front end: its trade route chooser shows `tradeRouteLength()`, the minimum; a route's own length is `tradeRouteDuration` of its way's plots.

Results:
- 128 AI games (Small, 6 AI, turn 200) against main: Gold falls 230.9 to 217.9 (-13.1 ± 6.3); science -0.1 ± 1.1, culture +0.4 ± 0.6, production -0.7 ± 1.4 and population +0.1 ± 0.4 are within noise. Eight 150-turn 6-AI games take about the same time (11.4 to 12.7 s against 11.3 to 11.6 s, three runs each).
- 8 long AI games in 8 setups (up to Huge, 400 turns): no crash or replay mismatch; all 8 play out differently.
- Tests: `trade_routes_last_whole_round_trips` (ways of 0, 1, 3, 10 and 11 plots), `deeper_debt_disbands_more_units` (one, two and all four Spearmen by depth), `inspirations_count_toward_lifetime_culture` (an Inspiration against a Eureka); `a_trader_runs_a_route_lays_roads_and_comes_home` now expects an 8-plot route to last 32 turns. Mutation check: 9 mutants, all caught (the debt test was changed to end its turn exactly on −10, −20 and the Gold just above them to catch the last).

## Unsure, not changed

- Score: the data's era buildings (×1) and converted citizens or cities (×2) items; the spec does not say what they count, and the core leaves them out.
- Whether debt costs an Amenity from the first Gold below 0 (the data's `GOLD_NEGATIVE_BALANCE_AMENITY_LOSS_LINE` of 0, which the core follows) or from each full 10 Gold (the spec's wording).
- How long one leg of a route takes when the Trader's way runs on roads (the core counts plots).
