# Record: the leader's loyalty effects (leader doc §1, §3, §5)

Status: done, 2026-10-09. Previous: `2026-10-09-city-rules.md`. The cities audit found three loyalty rules from the leader doc that were never built, because they waited on the loyalty system (built since in step 4). Chosen by Claude under James's standing consent; the amounts are Sovereign tuning.

## Rules

- **Presence aura (§1):** the city whose land the leader stands on gains `LEADER_AURA_LOYALTY` (4) loyalty a turn (`Game::loyaltyPerTurn`). A plot is the city's land when `Plot::city` names it, so the leader in another of its own cities steadies that one instead.
- **Statesman (§3, "more loyalty pressure"):** Wary and Spymaster each add +2 to that aura through a new `AURA_LOYALTY` promotion effect (`UnitEffectKind::AuraLoyalty`), on top of their assassin defence.
- **When the leader falls (§5):** a killed leader (in battle or by an assassin) costs every one of the empire's cities `LEADER_LOSS_LOYALTY` (20) loyalty at once, never below 0, and the empire `LEADER_LOSS_ERA_SCORE` (3) era score, never below 0 (`Game::successionShock`). A captured leader costs nothing more while the empire waits; abandoning the captive costs the heavier `LEADER_ABANDON_LOYALTY` (30) and the same era score. Regicide ends the game for that player before any of this.

## Results

- 128 AI games (Small, 6 AI, turn 200) against #243: no measurable change (17 of 128 games end differently at all). Inferred: AI leaders mostly stay in a capital already at full loyalty, and few fall before turn 200.
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; 5 setups saved and reloaded at 8 cuts end on the straight game's state.
- The 30-turn golden game's state hash is unchanged; the rules checksum changes (new globals and promotion effects).

## Tests

- `presence_aura_steadies_the_city_it_stands_in` (only that city, own cities only, +2 per Statesman promotion, a Builder-King promotion adds nothing), `a_slain_leader_shakes_every_city_and_the_era` (loyalty floored at 0, the enemy's cities untouched, era score floored at 0 and taken from the total too), `a_captured_leader_holds_the_throne_until_abandoned` (no cost at capture; the heavier cost on abandoning).
- Mutation check: 16 mutants (each part of the aura, each promotion's effect, each loss rule and floor, capture and abandonment swapped); the tests catch all 16 (the capture check right after the capture was added to catch the last one).

## Left open

- **Ransom (§5):** a captured leader is a deal-screen item to be ransomed for gold, cities or peace. Not built yet: today the only way out is abandoning the captive, and the AI does so at once. The next PR.
- **Statesman's better citizen interactions (§3, §4)** and **governor-seeded successors (§5)** are not built.
- **Unreal:** the city panel shows the loyalty total, which now includes the aura; nothing names the aura as its source.
