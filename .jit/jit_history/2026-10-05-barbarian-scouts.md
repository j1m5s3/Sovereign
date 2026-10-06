# Plan: barbarian scouts (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-diplomatic-access.md`. Chosen by Claude under James's standing consent: barbarian camps' scouting from 01, which the core skipped (camps raided on boldness alone). One milestone.

Specs: 01-map-and-terrain (Barbarians); data/barbarians-goody-huts (BARBARIAN_BOLDNESS_PER_SCOUT_LOST).

## Milestone

1. **Done** (`Camp::alerted`, `Camp::scoutSaw`, save version 54; `Game::releaseScout`, `isBarbarianScout`, `barbarianScoutAct`):
   - **Scouting.** A camp placed by `placeCamps` starts unalerted and sends out a Scout (UNIT_SCOUT). The Scout wanders to random land plots within 10 of its camp until a non-barbarian city is within its sight and line of sight.
   - **Alerting.** It then heads home, and reaching its camp, or a plot next to it, alerts the camp.
   - **Before that.** An unalerted camp's units fight units that come at them but neither raid nor attack cities, however bold the camp is.
   - **A lost Scout.** It costs the camp BARBARIAN_BOLDNESS_PER_SCOUT_LOST (-5) boldness and its news. When the spawn timer next runs out, the camp sends a new Scout before any other unit.
   - **Exceptions.** A camp with no free land plot beside it is alerted from the start. Camps made by scenarios and tests also start alerted, so older setups behave as before.

## Decisions (Claude's recommendations; James gave standing consent)

- Naval tribes scout on land too; Civ VI's per-tribe scout classes and the difficulty-based raid and attack force compositions are not modelled (camps still release one unit per spawn timer).
