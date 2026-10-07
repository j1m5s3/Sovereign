# Record: the AI's development planner (step 6, "everything else")

Status: done, 2026-10-07 (#132, #196). Previous: `jit_history/2026-10-06-spec-audit-part-2.md`; the pace study it follows up: `jit_history/2026-10-06-ai-pace.md`. Chosen by Claude under James's standing consent.

At turn 200 the AI is in era 2.4 (Medieval to Renaissance) against a target of the Industrial era. The pace study found that the core's rules match Civ VI and that weight tuning has stopped helping.

A fresh diagnosis (seed 1, turn 150, 40 major cities) shows:
- cities average population 4.8 against 5.6 housing, and 16 of 40 sit at their housing cap;
- three of six civs have no Builders at all;
- treasuries hold 180–350 Gold unspent;
- science is 5.8 a city.

## Method

Every change is measured with `sovsim --bench 8 --players 6 --size MAPSIZE_SMALL --turns 200` against a build of main, and kept only if it gains. Noise is about ±0.4 cities and ±5 science.

## Milestones (one PR, or a record of what was dropped)

1. **Done (kept): Builders and housing:**
   - a Builder floor that counts worked plots left unimproved;
   - farms, Granaries, Water Mills and Aqueducts before a city reaches its housing cap, not after.
2. **Done (kept): Gold that works:** the purchase reserve falls to 30 + 5 per city (was 60 + 15), so idle Gold buys Builders, Settlers and buildings sooner.
   - Also kept: Builders choose the improvement worth most on the plot: the resource's own improvement first, then yields, and housing for a city near its cap.
3. **Done (kept): science cities.** Settler pumps (X3) were dropped earlier as neutral.
   - A city of population 4 or more weighs its Campus +150 and the Campus's buildings +100.
   - Research weighs +8 the techs that open a science building.
4. **Dropped: opening build order.** Both variants moved nothing beyond noise, so the opening stays as it was (Slinger, Settler, Builder, Settler):
   - O1, the first two Settlers ahead of anything but a guard;
   - O2, the cheapest melee unit as the first guard.

## Results (8 seeds, turn 200)

| Build | cities | pop | techs | civics | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|
| main | 7.2 | 41.7 | 26.1 | 18.6 | 54.8 | 37.3 | 82.9 | 206 |
| X1 Builders, housing | 7.5 | 44.0 | 26.1 | 18.6 | 55.0 | 36.8 | 89.9 | 218 |
| X1 + X2 reserve | 7.4 | 43.9 | 26.3 | 19.0 | 59.9 | 38.4 | 89.0 | 170 |
| X1 + X3 settler pumps (dropped) | 7.4 | 44.3 | 26.0 | 18.5 | 56.2 | 37.1 | 87.7 | 231 |
| X1 + X5 improvement choice | 7.6 | 45.0 | 26.4 | 18.8 | 58.1 | 39.0 | 89.5 | 198 |
| X1 + X2 + X5 (kept) | 7.6 | 44.6 | 26.8 | 19.1 | 61.1 | 40.0 | 93.5 | 156 |

Science +11%, production +13% and population +7%, but era 2.6 stays.

Against the new main (after #195; 8 seeds, turn 200):

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 7.5 | 44.5 | 27.7 | 20.7 | 2.7 | 56.8 | 49.2 | 95.2 | 174 |
| R1 Campus in cities of 4+ | 7.6 | 44.5 | 28.8 | 20.6 | 2.8 | 65.8 | 47.8 | 94.3 | 161 |
| R2 research toward science buildings | 7.2 | 43.7 | 28.0 | 20.6 | 2.7 | 57.7 | 48.7 | 97.3 | 175 |
| R1 + R2 (kept) | 7.7 | 45.6 | 28.9 | 20.7 | 2.9 | 69.2 | 49.7 | 95.8 | 171 |
| R1 from population 3, + R2 | 7.1 | 43.3 | 29.1 | 20.7 | 2.9 | 68.4 | 48.9 | 92.3 | 157 |
| R1 doubled, + R2 | 7.2 | 42.8 | 28.9 | 20.4 | 2.9 | 69.2 | 43.8 | 88.6 | 163 |

Opening variants (8 seeds, turn 200, on top of R1 + R2):

| Build | cities | pop | techs | era | science | prod |
|---|---|---|---|---|---|---|
| R1 + R2 | 7.7 | 45.6 | 28.9 | 2.9 | 69.2 | 95.8 |
| + O1 Settlers first | 7.7 | 45.3 | 28.8 | 2.9 | 68.6 | 98.9 |
| + O2 cheap guard | 7.8 | 45.5 | 28.9 | 2.8 | 68.9 | 95.8 |
| + O1 + O2 | 7.7 | 45.2 | 28.8 | 2.8 | 68.3 | 99.0 |

## Findings

- Across both PRs, science at turn 200 went from 54.8 to 69.2 on the newer main. Production and population rose too.
- The era went from 2.6–2.7 to 2.9. The Industrial target (era 4) is still about one era away.
- The levers that worked:
  - Builders and housing (population and production);
  - spending gold;
  - science cities (Campus in cities of 4+);
  - research toward science buildings.
- The levers that did not: Settler pumps, opening orders, and the earlier single-knob weights.
- What remains is probably city count (7.7 against Civ VI's usual 10–12 on Small by turn 200) and city size. Both are bounded by sites the map offers and by housing. That would be its own study of site choice and the settle-site scorer.
