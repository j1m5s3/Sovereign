# Record: the AI researches improvements by the plots its cities would put them on (AI pace)

Status: done, 2026-10-09. Found by Claude: a count of the plots AI cities work (below) after the war-target and unit-placement fixes. Chosen by Claude under James's standing consent.

## What was wrong

- **Every improvement counted the same toward research.** `unlockValue` (ai.cpp), which weighs what a tech or civic unlocks, gave each improvement 3 points, whatever its yields and whether our cities had a plot for it, and nothing for a tech's bonus to an improvement (+1 Production to Mines at Apprenticeship, +1 Food to Pastures at Stirrups, +1 Production to Lumber Mills at Steel).
- In 16 AI games on main (Small, 6 AI, seeds 1000-1015), at turn 100, 508 of the 1717 plots the major civs' cities worked (30%) were woods or rainforest without an improvement, and none of their owners had Construction (the Lumber Mill's tech, +2 Production).

## Fix

- An improvement a tech or civic unlocks is worth half its yields (`worth`, the posture's yield weights) for each plot our cities hold that it could take, a plot they work counting twice, up to `kImprovementPlots` (16): a plot without an improvement, with the improvement's terrain, feature or seen resource (a seen resource takes only its own improvements, as `Game::improvementFits` has it), on its kind of ground (land or water), and not a city, district or wonder. A tech's bonus to an improvement counts the same way for each plot that has the improvement. The 3 points stay.
- Improvements only a special unit builds (Forts, Airstrips...), those for some plots only (beside a river or the coast, at the border), a governor's, those of city-states and those of other civs keep their 3 points only.
- Tried on seeds 1-128 against main (before #239), by the points a plot adds: an eighth of the yields gave science +2.5 and production +3.6 at turn 200, a quarter +3.9 and +6.0, a half +5.0 and +8.1, all of them +5.0 and +5.9, twice +5.3 and +7.5. A cap of 32 plots gave the same as 16.

## Results

256 seeds (Small, 6 AI, turn 200; seeds 129-384, run after the choice), each game set against the same seed on main (#239):

| Build | cities | pop | techs | civics | era | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|---|
| main | 8.7 | 61.7 | 32.6 | 23.1 | 3.4 | 122.8 | 71.1 | 164.5 | 204.9 |
| this | 8.8 | 63.0 | 33.1 | 22.9 | 3.5 | 127.6 | 74.1 | 170.3 | 213.8 |
| change | +0.07 ± 0.04 | +1.24 ± 0.27 | +0.48 ± 0.07 | -0.13 ± 0.06 | +0.08 ± 0.01 | +4.9 ± 0.8 | +3.1 ± 0.5 | +5.8 ± 1.0 | +9 ± 3 |

(± one standard error of the mean change.)

Worked plots, seeds 1000-1015: at turn 100, 104 of the 511 bare woods the civs' cities work belong to a civ with Construction (none on main); at turn 200, 204 of 5620 worked plots are bare woods (240 of 5560 on main), 175 of them millable (200).

Speed: the 150-turn benchmark games (Small, 6 AI, seeds 1-8) take 11.27 and 11.46 s of CPU against 10.91 and 10.80 s on main (two runs each, +3% and +6%). The new lines take 3 ms of each game (about 2,300 calls); the rest is the bigger empires.

Save and reload: 8 AI games in 5 setups, each saved and loaded once or twice, end on the straight game's state.

## Tests

- `ai_researches_toward_improvements_for_its_plots`: with woods on its city's plots (every Ancient tech and Horseback Riding known) the AI researches Construction, with Coal it cannot see under them too; with woods only beyond its borders or in another civ's city, Lumber Mills on them already, or Deer in them, another tech.
- `ai_researches_toward_bonuses_to_its_improvements`: with Pastures on Sheep around its city it researches Stirrups; with the Sheep unimproved or Mines on the hills instead, another tech.
- `ai_researches_only_toward_improvements_it_may_build`: Persia studies Early Empire for its Paradise Garden, England another civic; England passes over Games and Recreation (the City Park is a governor's); China passes over Construction (the Beacon Tower is built at its border only).
- Four of their checks fail on main: Construction for the woods, with Coal or without, Stirrups for the Pastures, and Early Empire for Persia.
- A mutation check of the new lines: 29 mutants, 16 fail the tests. Four of the rest change the two AI games replayed (seed 5 to turn 150, seed 77 to turn 200): dropping the 3 points (kept from main), the land-or-water check, the check that the plot is not a city's own, and a worked plot counting twice. Four change nothing on today's data: leaving out the exclusions of special units' improvements, city-states', and those beside a river or the coast (none of them has yields or a bonus). The scan's stop at 16 plots and the cap after it each make the other redundant. Three change nothing in the replays: leaving out the check that the plot belongs to a city (every plot a civ owns does), and those for a district or a wonder (their plots seldom fit an improvement newly unlocked).
- The helper `afterFirstTurn` (test_ai.cpp) sets up a capital of 6 with every tech of the eras before one known.

## Left open

- AI Builders leave woods unmilled: at turn 200, 175 of the plots AI cities work are bare woods or rainforest their owner could put a Lumber Mill on.
