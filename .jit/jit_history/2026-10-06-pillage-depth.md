# Plan: pillage depth (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-05-tourism-sources.md`. Chosen by Claude under James's standing consent, from the "not modelled" notes of earlier plans (pillage, late game, natural wonders). James can redirect at any point.

Specs: 05-units-and-combat (Pillage; Nuclear weapons), 03-districts-buildings-wonders (Pillage), 07 (Plunder); data/great-people (activation effects), data/units (Unit abilities).

## Milestones (one PR each)

1. **Done: Plunder bonuses and great people's unit abilities.** The generator types `+N% pillage/plunder yields` (`PLUNDER_PERCENT`) and the great people's `ability X [...] for your units` activation effects (`ABILITY`: Georgy Zhukov, Leif Erikson, Rajendra Chola, Francis Drake, Ching Shih, Horatio Nelson). A retired great person's ability applies to the civ's units of its classes, read from `Player::greatPeopleActivated` (no save change). Plunder bonuses raise pillage and coastal-raid yields (not the Heal) and the Gold from plundering a trade route.
2. **Done: Nuclear blasts pillage districts.** Every finished district in the blast but the City Center is pillaged (its buildings idle) for the fallout turns or the usual 10-turn repair, whichever is longer (`kPillagedDistrictTurns`, now in `state.h`).
3. **Done: Pillaging routes.** A land military unit at war pillages the road or railroad on an enemy plot once no improvement or district there is left to pillage (no plunder). A pillaged route (`Plot::routePillaged`, save version 62) moves as if it had no road and is not drawn in Unreal until a Builder or Military Engineer repairs it with `RepairImprovement` (no charge; on the player's land or no one's). A trader's new road, a railroad or a city replaces it. `pillaging costs only 1 movement` is generated as `CHEAP_PILLAGE` (Depredation, Superfortress, the Malon Raider): pillaging then costs PILLAGE_ADVANCED_MOVEMENT_COST.

## Decisions (Claude's recommendations; James gave standing consent)

- Milestone 1: abilities from great people sit beside the leader's and the policies' granted abilities in `Game::unitAbilities`. Zhukov's and Nelson's `where Land/Sea unit` is already covered by the ability's classes, so their flanking parses as plain `FLANKING_PERCENT`. Drake's Legacy (an England ability) stays unused: Sovereign's England has its own leader ability.
- Milestone 3: routes are pillaged last, after the plot's improvement and district, so one pillage takes one thing. The AI pillages roads like anything else in enemy land, and its Builders repair their own pillaged roads. Not modelled: pillaged buildings repaired by production (PILLAGE_BUILDING_REPAIR_PERCENT; Sovereign keeps its 10-turn self-repair of districts), air pillage, Norway's Thunderbolt plunder (Norway is not in the roster).
