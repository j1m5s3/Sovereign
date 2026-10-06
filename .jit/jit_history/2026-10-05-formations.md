# Plan: Corps and Armies (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-pillage.md`. Chosen by Claude under James's standing consent: unit formations from 05-units-and-combat, which the unit record already named (`formation`) and the core never had. One milestone, one PR.

Specs: 05-units-and-combat (Formations; combat strength modifiers); data/global-parameters (COMBAT_CORPS_/ARMY_STRENGTH_MODIFIER).

## Milestone

1. **Done: forming Corps, Fleets, Armies and Armadas:** `FormUnit` (command 57) merges a neighbouring twin into the unit: two singles make a Corps or Fleet (Nationalism), a Corps and a single an Army or Armada (Mobilization). Corps +10 strength, Army +17. AI merges twins as soon as it may; the Unreal build chooser offers it.

## Decisions (Claude's recommendations; James gave standing consent)

- Both units must be the player's, the same military land or naval type (not the leader), side by side, with moves left. The unit keeps the higher HP and XP of the two and its own promotions; forming takes its turn. `Unit::formation` (0, 1, 2; save version 45) survives upgrades.
- Not modelled: training Corps and Armies directly in cities with a Military Academy or Seaport (UNIT_CORPS_COST_MODIFIER 1.5, UNIT_ARMY_COST_MODIFIER 2.0), the anti-air bonus (+7) and the three-singles Army.
- AI: every turn, before moving its army, it merges each unit with the first neighbouring twin it may; at war or at peace.
