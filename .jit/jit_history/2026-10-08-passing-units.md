# Record: units pass other civs' units at peace (step 6, "everything else")

Status: done, 2026-10-08. Previous: `jit_history/2026-10-08-unit-stacking.md`, which left it open. Chosen by Claude under James's standing consent.

Units of different owners never share a plot, and a unit may move through one holding its own units of its layer, or any unit of a civ it is not at war with, when it gets past this turn (05: Stacking).

## What was wrong

- **Other civs' units blocked moves.** `Game::moveLimits` kept a unit out of every plot another player's unit held, at war or at peace, so a path went around a city-state's Warrior standing in a gap, or found none.

## Fix

- **Path search** (`Game::moveLimits`, `Game::searchPath`): a unit of a player at war still keeps a unit out (attacks and captures are their own commands). One of a player at peace marks its plot as one to pass but never end a move on (`MoveLimits::kPassOnly | kTheirs`), as our own units of the mover's layer do (`kPass`), whatever its layer. An order to its plot finds no path, as before, and a move reach leaves the plot out. A leader with its linked escort still keeps out of other civs' units: the pair moves as one and may not share a plot with another military unit (`Game::advanceUnit`).
- **Moving** (`Game::advanceUnit`, `Game::passOnly`, `Game::passOn`): a unit steps onto any plot it may only pass when its path takes it past to a free plot this turn; else it waits where it is, as its path plans. A single step's `Game::moveCost` into such a plot now has its cost.
- **A Heavy Chariot left on a passed plot.** A unit's full moves can hang on the plot its turn starts on (the Heavy Chariot has 3 on open ground, 2 in forest), and the search reads them where the unit stands. A chariot that set out from open ground passed a city-state's Warrior into forest, where the search, reckoning 2 moves a turn, found no way on, and its order ended there on the Warrior's plot (seed 1, turn 134; seven turns later the same chariot was left on its own Archer's). Now a unit standing among units it may only pass, with no path found, goes on past them along the path it took; beyond them its order goes on as any order does.
- Specs: 05 Stacking says passing covers any unit of a civ not at war; the leader brainstorm says a linked pair does not pass other civs' units.

## Results

8 AI games (Small, 6 AI) to turn 150, at each major civ's turn start:

| Build | orders whose path passes another civ's unit | plots shared with another player's unit | stacked plots of one owner |
|---|---|---|---|
| main | 0 of 1242 per game | 0.2 per game | 0 |
| this | 35.5 of 1265 per game | 0.1 per game | 0 |

The shared plots on both builds are a city-state's Archer that a levy handed to a major civ where it stood, on its city-state's Builder (seeds 5 and 7); no move ends on another player's unit. Orders that did not move stay as rare as on main: about 2 military unit-turns a game in 5920 (main 1 in 5892), none for Builders and Settlers.

64 seeds (Small, 6 AI, turn 200):

| Build | cities | pop | techs | civics | science | culture | prod |
|---|---|---|---|---|---|---|---|
| main | 8.0 | 47.6 | 28.9 | 20.6 | 71.6 | 48.7 | 112.7 |
| this | 8.0 | 47.6 | 28.9 | 20.7 | 71.6 | 49.7 | 114.1 |

Speed: the 8 games to turn 150 took 10.0 s of CPU on both builds (seed 5 alone 1.29 s against 1.10 s, its game having 171 units at the end against 144), and 20 turns of a Huge 12-AI game from turn 250 0.94 s against 0.93 s (medians of 5 runs). One path search in that Huge game costs about 5% more: a move reach 116 µs against 110, a path 35 µs against 33 (best of 9 runs over its 406 units, medians of 7 in three batches). Most of it is the passes themselves: with other civs' units kept out as before, the rest of the change costs about 3% of a reach.

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state. The four replay games of the speed-up checks replay their own command logs; their end states move, as for any rules change.

## Tests

- `game_moves_pass_other_players_units_at_peace`: another player's Warrior in a gap of a mountain wall, at peace. No order ends on its plot, a Warrior's or a Builder's (another layer), and a move reach leaves it out; a step into it has its cost. A Warrior passes it to the plot beyond; a Builder two plots back waits a plot short and passes next turn. With hills beyond, a Warrior has no path and a Scout gets past with 3 MP. At war the gap is closed. A Heavy Chariot passes it into forest and, though the search then finds no way on, goes on and passes one of ours in a second gap next turn.
- `a_linked_pair_keeps_out_of_other_players_units` (a lone leader passes them); `game_one_unit_per_tile`, whose check that another player's unit blocks a step compared two plots that are not neighbours, now checks the step's cost and that no order ends there; `ai_soak_takes_a_capital_and_replays` also counts plots shared with another player's units (none).
- Mutation check: 13 mutants, each condition of the change broken alone (war or peace either way, the single plot's check, the leader pair, the order's goal and the move reach, each part of `passOnly`, the chariot's way on and its condition, foreign cities). All 13 are caught.

## Left open

- The search reads a unit's full moves where it stands for every turn of a path, though they can hang on where each turn starts (the Heavy Chariot and War Chariot on open ground, the Mandinka Lancer in desert, the Chasqui on roads, embarked units): such a path can be planned longer or shorter than it turns out. Passes are safe from it now.
- A linked leader and escort do not pass other civs' units.
- A levy hands a city-state's units to their suzerain where they stand, so one can share a plot with the city-state's own civilian.
