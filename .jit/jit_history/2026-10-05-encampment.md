# Plan: Encampment combat (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-specialists.md`. Chosen by Claude under James's standing consent: the Encampment's defence from 03-districts (listed as not modelled since the rules core: "Encampment combat (HP 100, strike, ZOC), city strength +2 per district"). One milestone, one PR.

Specs: 03-districts-buildings-wonders (Defense); data/districts (Encampment HP 100, attack range 2).

## Milestone

1. **Done: the Encampment fights:** once its city has walls, a complete, unpillaged Encampment makes its own ranged strike each turn (`Command::encampmentStrike`, a `CityStrike` with arg 1; `City::encampmentStruck`, save version 47) at a visible enemy within its attack range (2) of its plot, with the city's strength; it exerts zone of control like a city; and an Encampment, Government Plaza or Diplomatic Quarter adds 2 to its city's strength (CityStrengthModifier, named in 03 but not in the extracted tables). AI cities fire it with their strikes; Ctrl+right-click with a city selected fires it in Unreal.

## Decisions (Claude's recommendations; James gave standing consent)

- Sovereign reading: an Encampment watches its strike range (sight 2 from its plot), so it can see what it may hit.
- Not modelled: the Encampment's own hit points and outer defences (attacks on it, its capture), which would need districts as combat targets.
