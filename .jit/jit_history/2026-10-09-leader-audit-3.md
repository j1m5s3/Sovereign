# Record: leader character rules the audit found off (part 3, body doubles)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-2.md`. Body doubles (leader doc section 8.6, decided "expensive"). Chosen by Claude under James's standing consent.

## Change

- **The Body Double** (`UNIT_BODY_DOUBLE` in `data/rules/leader.json`, `UnitType::bodyDouble`): unlocked by Diplomatic Service (the doc's proposal), 450 Production (twice a Spy's 225), 4 Gold upkeep (a Spy's). Trained or bought, it goes off the map into `Player::bodyDoubles` (save version 92), at most `BODY_DOUBLE_MAX` (1) at a time; `Game::goldPerTurn` charges its upkeep.
- **An assassin's strike can fall on it:** when an assassin's attack succeeds against a ruler whose civ keeps a double, `BODY_DOUBLE_PERCENT` (50) of the time the double dies instead. The ruler is unhurt, the assassin comes home without a level, and the sender is known as for a hit (the -15 opinion memory and the sender grievance from part 1). A new event, `EventKind::AssassinKilledDouble`, is in the chronicle ("an assassin from X killed a body double of the ruler of Y").
- **The AI** keeps one once an assassin has come for its ruler (a remembered assassination attempt), one in training at a time; its value (1000) is high because the double costs much Production, so it ranks per Production near a soldier.

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 2): 8 games differ, every measure within noise (largest t 1.8, population +0.01).
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from part 2. Time per game unchanged (two alternating pairs, within 1%).
- Tests: `body_doubles_are_trained_kept_and_paid_for` (Diplomatic Service needed, kept off the map, upkeep charged, one at a time, kept in a save), `an_assassin_can_strike_a_body_double` (over 30 seeds some strikes fall on the double, the ruler unhurt and the sender known), `an_ai_keeps_a_body_double_once_assassins_come` (with and without a remembered attempt). Mutation: 10 mutants, all caught.

## Unsure

- The doc says only "a chance"; 50% is Sovereign tuning. Doubles are not seen on the map, so an enemy cannot tell whether a civ has one.
- The Unreal UI shows the Body Double in the city's production list like any unit (inferred from how units are listed, not checked), but nothing yet shows how many doubles a civ keeps.

## Still open from the audit

Bodyguards (named companions; part of the doc's personal guard), XP for first visits, the rest of the promotion branch effects, in-person diplomacy, duel war score (see part 1).
