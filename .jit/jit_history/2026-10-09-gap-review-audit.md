# Record: gap review decisions in the rules core

Status: done, 2026-10-09. An audit of `specs/sovereign/open-gaps-review.md` (the decisions James adopted on 2026-10-04) against the core found four gaps. One, fogged plots showing live state (gap 6), was already being fixed (`2026-10-09-retention-audit-3.md`); this part fixes the other three. Chosen by Claude under James's standing consent.

## Change

- **Naval fights involving the leader auto-resolve** (gap 13: "Naval and air fights involving the leader auto-resolve"). Before, a ship's melee attack on an embarked leader stack, or a ship's assault on a coastal city holding the leader, waited for a live battle. Now `Game::liveBattleSide` and `Game::liveAssaultSide` return no side when either fighter is at sea (`Game::atSea`: a sea-domain unit, or a land unit embarked), and the fight resolves with Civ math at once. Air units never melee, so air strikes already resolved this way.
- **A leader beaten at sea dies** (gap 13: "A sunk transport with the leader aboard means death"). Before, a ship beating an embarked leader in melee captured it and held it for ransom. Now an embarked leader beaten in melee is lost, not captured, and the succession starts. Civ has no transport units, so the embarked leader stands for its transport.
- **A leader hurt by barbarians in a live battle goes home** (gap 8: barbarians "can wound the leader but not kill or capture it (it retreats to the capital at low HP)"). Before, the retreat ran only when the leader itself fought; a live battle's wound to a leader beside its escort could leave it on 1 to 30 HP in the field. Now that wound is followed by the same retreat check (`Game::barbarianWound`, `LEADER_RETREAT_HP`) when barbarians are on the other side.

## Checks

- Tests: `naval_fights_with_the_leader_auto_resolve` (a Galley sinks an embarked leader at once: no captor, succession pending), `an_assault_from_the_sea_on_the_leaders_city_auto_resolves`, `a_leader_hurt_in_a_live_battle_with_barbarians_goes_home`.
- Soak (8 long games with save/reload cuts): no crash, replay mismatch or reload drift; the games play out as with #297 (the AI and the soak bots fight no live battles).
- Mutation: 6 mutants, 5 caught; the one missed (dropping the "not the leader's own side" check on the barbarian foe) is equivalent, since a barbarian player has no leader.

## Already in the core (no change)

Gap 8's other rules (leader on the start plot, assassins at Political Philosophy, the Palace moving), gaps 7 and 11 (succession), 12 (difficulty), 14 (mounts, #297), 15 (mod data layers), 17 (a pending battle saved), 20 (policy change cost, Deity +4).
