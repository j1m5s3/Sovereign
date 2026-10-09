# Record: mounts and map moves (retention and world-scale audit, part 2)

Status: done, 2026-10-09. Previous: `2026-10-09-retention-audit.md` (part 1, which lists the audit's findings). This part fixes the mount rule. Chosen by Claude under James's standing consent.

## Change

- **A mount no longer adds map moves** (world-scale, decided in the gap review: "Walking is for inside a hex ... A mount speeds up walking inside a hex but does not change map moves"). Before, a Horse, Warhorse or Staff car added 2 to the leader's map moves (`Game::maxMoves`). Now only armor changes map moves (Plate -1). A mount's `moves` in `leader.json` stays as its walking speed inside a hex for the street scene, and it still costs twice its unit's upkeep (leader doc §8.8).
- The AI never equips mounts (it buys weapons and armor only), so AI games are unchanged.

## Checks

- Soak (8 long games with save/reload cuts): no crash, replay mismatch or reload drift; 1 of 8 end states differs from #291, a game with random-bot seats (inferred: the bot equips mounts). The AI pace games have no mounts, so they were not rerun.
- Tests: `equip_gear_needs_city_tech_and_gold` (a horse leaves map moves at 2), `leader_strength_comes_from_gear` (Plate still costs a move with a horse on). Mutation: 2 mutants, both caught.

## Unsure

- With no map effect, a mount pays off only when walking inside a hex and in live battles, which are Unreal's; its upkeep now buys nothing on the map. The Unreal gear panel still labels mounts "+2 moves" (`SovPlayerController.cpp`), left to the UI work.
