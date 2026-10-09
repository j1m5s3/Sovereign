# Record: leader character rules the audit found off (part 5)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-audit-4.md`. The Warlord branch's last missing effect. Chosen by Claude under James's standing consent.

## Change

- **A Marshal leads its escort as a formation** (doc section 3, Warlord: "can lead a Corps/Army with its escort"). Marshal now carries `LEADS_ESCORT` 1 (`UnitEffectKind::LeadsEscort`). The military unit linked to the ruler as its escort, while on the ruler's plot, fights one formation larger: a single unit with the Corps bonus (+10), a Corps with the Army bonus (+17, so +7 more), an Army as it is. Sovereign reading: it changes combat strength only; the unit's own formation, and with it conditions on being in a formation, are unchanged.
- The doc's other Warlord effects were already built: Weapon Master's +5 strength is the "better weapon handling" (the doc's "with the current weapon type" is read as whatever weapon is equipped), Marshal's aura +2 and assassin defence +5 are the "stronger aura" and "bonus vs assassins".

## Checks

- Pace (128 AI games, 6 civs on Small maps to turn 200, against part 4): every game identical. Inferred: no AI ruler takes Marshal by then.
- Soak (8 long AI games with save/reload cuts, to turns 300-400): no crash, replay mismatch or reload drift; all 8 end states differ from part 4. Time per game: five alternating pairs on identical Small games, within 1.5% overall (the first two pairs read 5% slower, the next three 1% faster on balance; machine noise).
- Tests: `a_marshal_leads_its_escort_as_a_formation` (single, Corps and Army escorts; no step without Marshal, unlinked, or linked but on another plot). Mutation: 4 mutants; 3 caught (the plot check after its test case was added), 1 equivalent (a cap at Army that the bonus code already applies), so that cap was removed.

## Still open from the audit

Bodyguards (named companions), in-person diplomacy, duel war score (see part 1).
