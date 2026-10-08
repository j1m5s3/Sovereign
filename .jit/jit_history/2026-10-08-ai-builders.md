# Record: AI Builders that work (step 6, "everything else")

Status: done, 2026-10-08. Previous: `jit_history/2026-10-07-ai-planner.md` (the Builder floor and the improvement choice). Chosen by Claude under James's standing consent, after the rules gaps list (#217–#223).

Builders take a seventh of the AI's production (14.4% to turn 150, 16 seeds), yet on 16 seeds to turn 100 they stood idle in 55% of their turns and built in 11%. Each Builder turn was traced (`src/ai.cpp` `build`, `production`).

## Causes and fixes

1. **Plots out of reach.** A Builder's best plot can lie out of its reach: a sea resource before it may embark, or another landmass. The move failed and the Builder skipped its turn, every turn, while that plot stayed its best. Now the first failed move finds the Builder's move reach (`Game::moveReach`), and only plots in it are tried after; it gives up after six failed moves.
2. **Waiting out of moves.** Found while fixing 1. A move order's step at the start of a turn can leave a Builder on its plot with no moves to build. Fix 1 alone then sent it on to the next plot, where the same happened: two plots, one Builder, 25 turns of ping-pong. Fix 1 without this lowered production at turn 100 from 29.1 to 27.4. Now a Builder whose best plot is the one it stands on waits there and builds next turn, as it did before fix 1.
3. **Stale orders.** A Builder on its way kept its order after another Builder had improved (or mended) the plot. One such order held a Builder 35 turns (seed 1, turns 23–58). Now the order is dropped once the plot no longer wants work, and the Builder turns to other work.
4. **Builders without work.** The Builder floor (a count by cities, from the planner) trained Builders when no plot was left to improve. At turn 30 of seed 1, one civ had three Builders, seven charges and nothing to improve. Now a city trains one only while the plots Builders could improve within three of our cities outnumber the charges our Builders carry, those in training included.
5. **Claims split.** A Builder's plot no longer keeps Settlers off city sites within three of it. `View::claimed` holds Settlers' sites; `View::works` holds Builders' plots, and Builders still split the work over them.

## Results (64 seeds, Small, 6 AI, turn 200)

| Build | cities | pop | techs | civics | science | culture | prod |
|---|---|---|---|---|---|---|---|
| main | 7.9 | 44.4 | 28.2 | 20.1 | 64.1 | 44.1 | 89.8 |
| 3 stale orders only | 7.7 | 43.5 | 28.0 | 20.0 | 62.8 | 43.5 | 88.1 |
| 4 Builders without work only | 7.7 | 43.5 | 28.2 | 20.1 | 62.9 | 43.4 | 86.1 |
| 1 + 3 + 5, without 2 | 7.6 | 43.5 | 27.6 | 20.0 | 60.6 | 43.3 | 92.6 |
| 1 + 2 + 5 | 7.8 | 46.7 | 28.7 | 20.5 | 70.8 | 48.0 | 110.9 |
| all five (kept) | 8.1 | 47.9 | 28.8 | 20.7 | 70.8 | 48.9 | 112.1 |

Checked on games not looked at while tuning:

| Games | Build | cities | pop | techs | science | culture | prod |
|---|---|---|---|---|---|---|---|
| seeds 65–128, Small, 6 AI | main | 7.7 | 43.5 | 28.1 | 62.2 | 43.6 | 87.8 |
| | kept | 8.0 | 47.7 | 29.1 | 73.0 | 49.5 | 113.9 |
| seeds 1–24, Standard, 8 AI | main | 7.8 | 45.2 | 28.5 | 66.0 | 47.5 | 91.9 |
| | kept | 8.1 | 49.7 | 29.2 | 74.8 | 51.1 | 117.7 |

Builder turns (16 seeds, to turn 100):
- main: 33.1 Builders made per game; idle 55%, moving 34%, building 11% of their turns; 67.6 improvements built.
- kept: 31.1 Builders made; idle 27%, moving 56%, building 16%; 75.3 improvements built.

## Findings

- Science at turn 200 rose 10–17% and production 25–30%, with population up 8–10%. The era stays at 2.8–2.9.
- Fixes 3 and 4 alone moved nothing beyond noise (about ±0.25 cities and ±3 science at 64 seeds). They are kept because with Builders that work, charges run out faster, and these keep the Builders that are trained busy.
- **AI soak test:** now plays seed 87. Seed 27's game no longer sees a capital fall by turn 250. Over 100 seeds (Tiny, 4 AI, 250 turns) a capital fell in 55 games before and 61 after.
