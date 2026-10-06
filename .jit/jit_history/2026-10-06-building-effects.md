# Plan: building effects left out (step 6, "everything else")

Status: done, 2026-10-06. Previous: `jit_history/2026-10-06-unit-promotions.md`. Chosen by Claude under James's standing consent: a check of `buildings.md`'s Modifiers column against `modifiers.json` and the core found buildings whose effects nothing applied. One milestone, one PR.

Specs: 03-districts-buildings-wonders; data/buildings (Modifiers).

## Milestone

1. **Done:** hand-written modifiers:
   - Zoo: +1 Science on the city's rainforest and marsh.
   - Aquarium: +1 Science on coast plots with a resource, and on reefs. `PLOT_HAS_RESOURCE` without a ref now means any resource.
   - Pagoda: +1 Diplomatic Favor.
   - Shopping Mall +4 and Ferris Wheel +2 tourism (new `ADJUST_CITY_TOURISM`).
   - Consulate: +2 influence points.
   - Audience Chamber: +2 Amenities in cities with a governor, -2 loyalty in those without.
   - Ancestral Hall: +50% toward Settlers.
   - Warlord's Throne: +20% Production in captured cities (new requirement `CITY_CAPTURED`).

   And in code:
   - Ancestral Hall: a Builder in every city founded.
   - War Department: a unit that kills heals 20.
   - Grand Master's Chapel: land combat units can be bought with Faith, at their Gold price.
   - Mosque: +1 charge for religious units bought there.

## Decisions (Claude's recommendations; James gave standing consent)

- Warlord's Throne's "+20% Production (City Captured)" is read as a standing bonus in cities taken from other civs.
- Not modelled:
  - Foreign Ministry: city-state levies were not modelled yet (they followed in `2026-10-06-levies.md`, with its +4 and half-price levy).
  - Royal Society: Builders do not contribute to projects.
  - The Consulate's spy defence.
