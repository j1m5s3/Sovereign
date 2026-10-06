# Plan: specialists (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-formations.md`. Chosen by Claude under James's standing consent: citizens working district slots as specialists (02-cities, 03-districts), listed as not modelled since the rules core ("citizen slots and specialists"). One milestone, one PR.

Specs: 02-cities (Citizens and specialists); 03-districts-buildings-wonders (Citizen slots); data/districts (Specialist yields), data/buildings (Citizen slots, Specialist yields).

## Milestone

1. **Done: specialists:** buildings open citizen slots in their district (generated `citizenSlots`); each specialist earns the district's specialist yield (generated `specialistYields`, e.g. Campus +2 Science, Commercial Hub +4 Gold, Encampment +2 Gold +1 Production) plus its buildings' extras. `assignCitizens` (now public) weighs every specialist slot against every workable plot with the same citizen score and fills the best, plots winning ties; `CityDistrict::specialists` holds the count (save version 46). A pillaged district's slots are closed. The Unreal city panel lists specialists per district.

## Decisions (Claude's recommendations; James gave standing consent)

- Locked plots are kept; specialists are never locked (no manual specialist control yet).
- A specialist eats like any citizen (food is per population), so the citizen score's food weight keeps cities growing; the AI pace test still passes, and a 6-AI Deity game now ends at turn 327 instead of about 370 (specialists' Science speeds the race to victory).
- Not modelled: Great person points from specialists, per-specialist yield bonuses from policies and governors.
