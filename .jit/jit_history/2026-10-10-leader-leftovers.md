# Record: the leader doc's last open rules

Status: done, 2026-10-17 (begun 2026-10-10, paused a week). Previous: `2026-10-10-overview-audit.md`. The leader character audit (`2026-10-09-leader-audit.md`, parts 1-8) left two lines of specs/sovereign/leader-character-brainstorm.md unbuilt. Chosen by Claude under James's standing consent.

## Fixes

- **Defences against assassins** (doc section 6 "Defenses": counterspies in the city, Victor the Castellan, staying in the capital): while the ruler stands on one of its own city centres, `Game::leaderDefenseVsAssassin` adds `ASSASSIN_CAPITAL_DEFENSE` (10) in the capital, `ASSASSIN_COUNTERSPY_DEFENSE` (5) per level of each own spy at work there on Counterspy, and `ASSASSIN_VICTOR_DEFENSE` (10) plus `ASSASSIN_VICTOR_DEFENSE_PER_PROMOTION` (5) for each promotion past Redoubt for an established Victor. Sovereign tuning, hand-written in data/rules/leader.json. Escorts stay the main defence: these sit beside the guards' 50% of their strength, and an unguarded ruler in a city still counts as exposed.
- **Builder-King** (doc section 3: faster governor establishment, extra Great Person points while present): Overseer's `GOVERNOR_HASTE` 1 takes one more establishment turn a turn off a governor in the city the ruler stands in (`Game::processGovernors`), and Patron's `GREAT_PERSON_POINTS` 2 adds 2 points of each class that city already earns (`Game::greatPersonPointsPerTurn`, before the city's percent). New `UnitEffectKind::GovernorHaste` and `GreatPersonPoints`.

- **A crash in AI Traders and Builders** (found by this slice's long games, from #303): `ai::playTurn` sent fresh Traders and Builders out while walking `GameState::units` itself, so an order that removed a unit (a Builder's last charge, a Trader leaving on its route) left the walk on a moved vector; a Huge 12-AI game (seed 11) crashed at turn 276. The units are now listed first and looked up again by id.

## Results

- 128 AI games (Small, 6 AI, turn 200, Prince) against the overview audit: every figure within two standard errors (cities -0.08 ± 0.05, Gold +6 ± 5). The crash fix changed none of the 128.
- 8 long AI games in 8 setups end with no crash or replay mismatch (the Huge 12-AI game crashed before the fix); the saved and reloaded runs end on the straight game's state.
- Mutation: every mutant (11) caught.
- The 30-turn golden game's state hash file regenerated (rules checksum).
