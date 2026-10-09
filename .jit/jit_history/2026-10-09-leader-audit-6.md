# Record: leader character rules the audit found off (part 6, duels)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-5.md`. Chosen by Claude under James's standing consent.

## Change

- **A won duel is worth a large war score** (doc section 8.5: "Winning gives XP and a large war score; the loser is killed or captured"). The core has no war score; Civ VI's nearest measure is war weariness (08), so this is a Sovereign reading. When a ruler beats another ruler in battle (one dies or is taken, the other stands), `Game::duelWon` gives the loser's civ `DUEL_WAR_WEARINESS` (400, one amenity's worth, `WAR_WEARINESS_POINTS_FOR_AMENITY_LOSS`) against the winner, through `Game::addWarWeariness`, so the loser's policies and grievances still soften it. The winner's war weariness against the loser falls by as much.
- The rest of the rule was already built: the XP is the normal combat XP, and the loser is killed or, beaten in melee, captured (doc section 5).

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 5): identical files.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from part 5.
- Tests: `a_ruler_who_wins_a_duel_wins_war_score`, `no_duel_without_a_beaten_ruler` (a ruler beating a Warrior; two rulers both standing). Mutation: 5 mutants, all caught (two after the second test was added and tightened).

## Still open from the audit

Bodyguards (named companions with levels and traits, from units, Great Generals and governors) and in-person diplomacy (the ruler at a foreign capital or city-state improving deal acceptance or envoys); both are larger designs.
