# Plan: the Encampment as a combat target (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-pillage-depth.md`. Chosen by Claude under James's standing consent: the gap left by `2026-10-05-encampment.md` ("Not modelled: the Encampment's own hit points and outer defences (attacks on it, its capture), which would need districts as combat targets"). One milestone, one PR.

Specs: 05-units-and-combat (Combat; City combat: "Cities and walls have their own HP (city center 200, Encampment 100 ...)"), 03-districts-buildings-wonders (Defense); data/districts (Encampment HP 100).

## Milestone

1. **Done: attacks on an Encampment:** a standing Encampment (complete, not pillaged) is a combat target like a city: melee and ranged attacks on its plot hit it, whoever stands there, and enemy units at war cannot walk into it. It has its own hit points (`Districts.hp`, 100) and the city's walls as outer defences, which take hits first with the usual wall percentages (support units next to the Encampment count). It defends with its city's strength, wounded by its own hit points instead of the city's. Damage is stored as `CityDistrict::damage` and `wallDamage` (save version 63), so a district starts at full health. It heals with its city (20 a turn unless the city is besieged; its walls 10 a turn after the quiet spell). At 0 HP it falls: pillaged for the usual 10 turns (no strike, no ZOC), and repaired at full strength. AI: values an Encampment below a city and checks interception over the Encampment's plot. Unreal: right-click attacks it; the city panel shows its HP and walls.

## Decisions (Claude's recommendations; James gave standing consent)

- Sovereign reading of a fallen Encampment: Civ VI's rule for one at 0 HP is unverified in the specs, so it falls to pillage, as the 10-turn self-repair of districts already does, rather than being captured or occupied. The attacker stays where it was.
- An Encampment fight never goes live (leader doc §9 sends only cities and leader stacks to the battle scene).
- The city's own HP and walls are separate from the Encampment's, and a hit on one does not touch the other. Both use the same `lastAttackedTurn` for the wall-repair cooldown.
