# Plan: pillage depth (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-05-tourism-sources.md`. Chosen by Claude under James's standing consent, from the "not modelled" notes of earlier plans (pillage, late game, natural wonders). James can redirect at any point.

Specs: 05-units-and-combat (Pillage; Nuclear weapons), 03-districts-buildings-wonders (Pillage), 07 (Plunder); data/great-people (activation effects), data/units (Unit abilities).

## Milestones (one PR each)

1. **Done: Plunder bonuses and great people's unit abilities.** The generator types `+N% pillage/plunder yields` (`PLUNDER_PERCENT`) and the great people's `ability X [...] for your units` activation effects (`ABILITY`: Georgy Zhukov, Leif Erikson, Rajendra Chola, Francis Drake, Ching Shih, Horatio Nelson). A retired great person's ability applies to the civ's units of its classes, read from `Player::greatPeopleActivated` (no save change). Plunder bonuses raise pillage and coastal-raid yields (not the Heal) and the Gold from plundering a trade route.
2. **Nuclear blasts pillage districts.** Every district in the blast but the City Center is pillaged for the fallout turns (or the usual repair time if longer).
3. **Pillaging routes.** A military unit at war pillages the road or railroad on an enemy plot that has no improvement or district left to pillage; a pillaged route gives no movement until a Builder or Military Engineer repairs it (no charge). Promotions with the advanced pillage cost (1 move) if the data has them.

## Decisions (Claude's recommendations; James gave standing consent)

- Milestone 1: abilities from great people sit beside the leader's and the policies' granted abilities in `Game::unitAbilities`. Zhukov's and Nelson's `where Land/Sea unit` is already covered by the ability's classes, so their flanking parses as plain `FLANKING_PERCENT`. Drake's Legacy (an England ability) stays unused: Sovereign's England has its own leader ability.
