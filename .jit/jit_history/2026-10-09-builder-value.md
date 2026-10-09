# Record: AI cities value a Builder by all our cities' unimproved plots (step 6, "everything else")

Status: done, 2026-10-09. Previous: the AI Builders follow-up in `jit_history/2026-10-09-sea-improvements.md`. Chosen by Claude under James's standing consent: worked plots left unimproved were one of the causes of the AI's pace left after `2026-10-09-housing.md`.

## What was wrong

- **A city valued a Builder by its own unimproved plots only** (`production`, ai.cpp: 160, and 40 for each plot it works that a Builder could improve, up to 6). A Builder trained in one city works for all of them, so a city whose own plots were improved valued one at 160 however many plots our other cities worked unimproved.
- In 8 AI games (Small, 6 AI) from turn 120 to 200, 472 of 2823 production choices were made while the civ's cities worked 6 or more unimproved plots beyond the charges its Builders carried; a Builder was chosen in 118 of them. Of the 562 choices that passed a Builder over with 3 or more such plots, the most common picks were a University, Library, Horseman, Campus, Ancient Walls and Heavy Chariot, and 380 were made by cities with no or one unimproved plot of their own. At turn 150 the major civs each worked 7.2 plots a Builder could improve, with 2.2 Builders carrying 4.1 charges (32 games).

## Fix

- `production` counts the plots our cities work that a Builder could improve, in the loop that already counts the work left (`workedWork`). A Builder is worth 160, and 40 a plot up to 6, for this city's unimproved plots or for all our cities' beyond the charges our Builders carry (those in training too), whichever are more.
- Tried and left out: valuing a Builder by all our cities' plots beyond the charges alone, without the city's own (128 games: science +0.1 ± 1.3, production -1.1 ± 1.6).

## Results

256 seeds (Small, 6 AI, turn 200), each game set against the same seed on main:

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.2 | 55.4 | 31.1 | 21.8 | 3.2 | 105.0 | 61.1 | 143.9 | 190 |
| this | 8.2 | 56.1 | 31.3 | 22.0 | 3.2 | 106.8 | 62.2 | 147.2 | 193 |
| change | -0.05 ± 0.06 | +0.7 ± 0.4 | +0.19 ± 0.08 | +0.18 ± 0.06 | +0.01 ± 0.01 | +1.8 ± 0.8 | +1.2 ± 0.5 | +3.3 ± 1.2 | +3 ± 3 |

(± one standard error of the mean change.) The change was chosen on seeds 1-128 (science +1.0 ± 1.2, production +2.4 ± 1.7); seeds 129-256, run after, gave science +2.6 ± 1.2 and production +4.2 ± 1.7.

Worked plots a Builder could improve, per major civ (seeds 1-32): 7.2 at turn 150 and 5.7 at turn 200 on main; 6.4 and 4.4 with this change. Builders on the map at turn 200: 2.0 on main, 2.3 with it.

Speed: the 150-turn benchmark game (seed 5) takes 1.10 s of CPU against 1.07 s on main (4 runs each; a different game by then, with more Builders). The worked plots are counted in the loop that already counts the work left, on its builderCanImprove calls.

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

## Tests

- `ai_trains_builders_for_plots_other_cities_work`: a capital whose own plots are farmed picks what to make while a second city works unimproved plots. With four such plots and no Builder it trains one (on main it trains a Slinger); with four and a Builder of three charges at work there it does not; working three unimproved plots of its own, it trains one beside that Builder.
- A mutation check (8 small changes to the new code) fails the tests for all 8.

## Left open

- Worked plots still wait for Builders: 4.4 a civ at turn 200, while their Builders carry 4.5 charges. That points to Builders on their way and plots they do not reach (land across water, see `2026-10-09-sea-improvements.md`) rather than too few Builders (inferred, not traced).
