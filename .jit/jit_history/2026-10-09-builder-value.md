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

## Follow-up: AI Builders improve worked plots before Bonus resources (2026-10-09)

Chosen by Claude under James's standing consent, from the point left open above.

### What was wrong

- `build` (ai.cpp) ranks the plots a Builder could improve by their worth times 10, less 15 a plot of distance. A plot was worth 10, 20 more for a resource it shows, 30 more again for a strategic one, and 10 more if a city works it. So an unworked Bonus resource (Wheat, Rice, Cattle, Sheep, Stone, Copper, Fish and the like) was worth 30, more than a plot a city works (20). Yet a Bonus resource yields the same improved or not (`plotYields`, city.cpp, adds a resource's yields once its tech is known, whatever stands on the plot): its Farm or Pasture gains no more than one anywhere else, and nothing until a city works the plot. A luxury's Amenities and a strategic resource's stock come once it is improved, worked or not.
- In 32 games on PR #233's code, the major civs each worked 6.4 plots at turn 150 that a Builder could improve. For 4.1 of them a Builder could get there over land but none was heading there; 1.1 had a Builder on them or on its way, 1.0 were in civs with no Builder, and 0.1 were out of reach. In 8 of the games, from turn 100 on, the Builders spent 58% of their turns walking and 18% building. That unworked Bonus resources outranked the worked plots was read from the weights, not traced.

### Fix

- A Bonus resource adds nothing to a plot's worth; a luxury keeps its 20 and a strategic resource its 50. The plots our cities work (20) now come before unworked Bonus resources (10), and a worked Bonus resource ranks with the other worked plots.
- Tried and left out: 30 rather than 10 for a worked plot (128 games against PR #233's code: science -0.5 ± 1.2, production +0.4 ± 1.9), which put worked plots above unworked luxuries too; 5 for a Bonus resource, as a tie-breaker (256 games against PR #233's code: science +0.5 ± 0.9, production +2.1 ± 1.1, gold -6.6 ± 3.0, where this change gave science +2.8 ± 0.9 and production +3.9 ± 1.2 on the same games); and keeping the 20 for a Bonus resource while improving it would still earn a boost (Irrigation, Masonry, the Wheel, Horseback Riding, Celestial Navigation; 512 games against this change: science -0.3 ± 0.6, production +0.2 ± 0.8).

### Results

512 seeds (Small, 6 AI, turn 200), each game set against the same seed on main:

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.20 | 56.0 | 31.21 | 22.03 | 3.22 | 106.1 | 62.1 | 146.1 | 193 |
| this | 8.32 | 56.3 | 31.50 | 22.17 | 3.26 | 109.0 | 63.1 | 150.1 | 192 |
| change | +0.12 ± 0.05 | +0.3 ± 0.3 | +0.28 ± 0.06 | +0.14 ± 0.04 | +0.05 ± 0.01 | +2.9 ± 0.6 | +1.0 ± 0.4 | +3.9 ± 0.9 | -0.6 ± 2.1 |

(± one standard error of the mean change.) The change was chosen on seeds 1-128 (science +4.4 ± 1.3, production +5.7 ± 1.7); seeds 129-512, run after, gave science +2.3 ± 0.7 and production +3.4 ± 1.0. It shows by turn 100 (256 games: culture +0.36 ± 0.08 on 11.9, production +0.4 ± 0.2 on 38.0, science +0.18 ± 0.10 on 18.8).

How many worked plots are improved hardly changed (64 games, per major civ): 10.2 of 17.2 at turn 100 on main and with this change, 24.7 and 24.9 of 34.3 at turn 150, with 6.8 and 6.6 more that a Builder could still improve. So the gain is in which plots are improved first (worked ones, which pay at once), not in how many (inferred).

Speed: the 150-turn benchmark game (seed 5) takes 1.09 s of CPU, as on main (4 runs each; a different game by then, with 52 cities against 40). The change adds one class test for each resource plot scored.

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

### Tests

- New `ai_builders_improve_worked_plots_before_bonus_resources`: a capital works a plot two east of it. Its Builder heads there before an unworked Wheat beside the capital (on main it farms the Wheat), and plants an unworked Wine beside the capital first.
- `ai_builders_pass_over_plots_others_stand_on`, `ai_builders_split_up_over_the_work` and `ai_builders_embark_for_sea_resources` failed with this change: they took Wheat or Fish for the best plot. They now use Wine (with Irrigation) and Pearls. `ai_builders_work_what_they_can_reach` still passed, but its Builder no longer tried the island's Wheat first (the land plots beside it now rank higher), so it uses Wine too.
- A mutation check (5 small changes to the weighting) fails the tests for 4. The one missed takes the 20 from strategic resources only; they still rank above every other plot on their own 30.

### Left open

- Worked plots still wait for Builders, as on main (32 games, per major civ): 6.6 at turn 150 and 4.6 at turn 200 that a Builder could improve, 4.8 and 3.1 of them in reach with no Builder on its way, while each civ has about 2 Builders carrying 4 charges, one of them on its way at a time. At turn 150, 4.0 of the 6.7 (16 games) are forests and jungles where a Lumber Mill is the only improvement. Tried on top of this change and left out (128 games each): a heavier distance weight, 25 or 40 a plot rather than 15 (science -2.9 ± 1.1 and -2.7 ± 1.3); a Builder valued higher, 60 a plot rather than 40 or a base of 240 rather than 160 (science -1.9 ± 1.3 and -3.3 ± 1.6); and a worked plot weighed by its best improvement's yields, which puts Lumber Mills before Farms (science -4.2 ± 1.1, population -1.7 ± 0.5, production -5.1 ± 1.6).
- Buenos Aires' suzerain bonus (each kind of improved Bonus resource is an Amenity there, improvements.cpp) is not weighed.
