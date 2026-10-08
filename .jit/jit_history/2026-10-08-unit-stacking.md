# Record: units never end a turn on their own units (step 6, "everything else")

Status: done, 2026-10-08. Previous: `jit_history/2026-10-08-ai-builders.md`, whose Builder traces showed it. Chosen by Claude under James's standing consent.

One unit per layer on a plot (05: Stacking): a unit may pass its owner's units of its layer but not stop on one.

## What was wrong

- **Stacked units.** A move order's path ignored the owner's own units, and a step onto one's plot was refused only when it used up the unit's moves. With moves to spare a unit stepped on, then stopped there when the next step cost more than it had left (hills, woods, a river), and the two shared the plot until its next turn. In 8 AI games (Small, 6 AI, to turn 150), 177.5 plots per game held two military units of one owner as its turn began, and 2.1 two civilians. Two Warriors stood fortified on one city for 24 turns (seed 1).
- **Orders that never moved.** A step that would have used up the moves there waited instead, while the path still led through the other unit: an order behind one of our units that stayed put waited for good. In the same games such orders stood still in 5.7% of Builders' turns, 4.2% of military units' and 2.9% of Settlers', one for 59 turns.
- **Encampments.** Path planning left out an enemy Encampment's zone of control, which stops a unit beside it (`Game::inEnemyZoc`); planned paths went on past it.

## Fix

- **Path search** (`Game::searchPath`): plots our units of the mover's layer hold are passed within a turn and never end one (`kOurs`; the order's end is planned as any plot, and the order ends before it while one of ours holds it). Such a plot keeps each arrival no other there beats (`Pass`: an earlier turn with as many moves or more), as an earlier arrival does not stand for a later one with more moves where no turn may end. From the plot before one, a pass may also start after waiting there for the next turn's moves. A unit standing on such a plot with moves left may not wait there either. Enemy zone of control there ends the move, so a unit may not pass one of ours beside an enemy at war; a search along a path or a move reach, which otherwise leaves zone of control out (it changes only the moves a path leaves), reads it the first time it steps onto one of ours.
- **Moving** (`Game::advanceUnit`, `Game::passOurs`): a unit steps onto our unit's plot only when its path takes it past to a free plot this turn; else it waits where it is, as its path plans; when our units hold the plots up to the order's end, the order ends.
- **Encampments** (`Game::markZoc`): an enemy Encampment marks zone of control as a city does.

## Results

8 AI games (Small, 6 AI) to turn 150, at each major civ's turn start:

| Build | stacked plots (military, civilian) per game | orders that did not move (Builders, military, Settlers) | longest wait |
|---|---|---|---|
| main | 177.5, 2.1 | 5.7%, 4.2%, 2.9% of their turns | 59 turns |
| this | 0, 0 | 0%, 0.02% (one turn), 0% | 1 turn |

64 seeds (Small, 6 AI, turn 200):

| Build | cities | pop | techs | civics | science | culture | prod |
|---|---|---|---|---|---|---|---|
| main | 8.1 | 47.9 | 28.8 | 20.7 | 70.8 | 48.9 | 112.1 |
| this | 8.0 | 47.6 | 28.9 | 20.6 | 71.6 | 48.7 | 112.7 |

Speed: the 150-turn benchmark game (seed 5) took 0.97 s of CPU against main's 0.99 s, and 20 turns of a Huge 12-AI game from turn 250 0.84 s against 0.83 s (medians of 5 runs). One path search in that Huge game costs more where its owner's units stand close: a move reach 99 µs against 91, a path 31 µs against 27 (best of 9 runs over its 406 units); trimming the zone-of-control and Encampment reads saved nothing measurable, so the cost is the passes themselves.

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state. The four replay games of the speed-up checks replay their own command logs; their end states move, as for any rules change.

## Tests

- `game_moves_never_end_on_our_units`: no path past a Builder in a gap with hills beyond; one after it steps back through the other's plot; a Warrior's move ending in the gap with the Builder (another layer); waiting a plot short and passing next turn as the path says; an order whose end another of ours took meanwhile ends a plot short; a Scout past two of ours in one move, a Warrior around them; a Horseman's earlier arrival with fewer moves kept beside a later one with more; a Warrior around one of ours before woods.
- `zone_of_control_ends_planned_moves_past_our_units`, `zone_of_control_closes_a_gap_our_unit_holds` (with the move reach agreeing), `units_embark_past_our_units_only_with_moves_left` (planned and moved), the Encampment's zone of control in a planned path (`an_encampment_strikes_and_holds_ground`, `an_encampment_stops_planned_moves_only_finished_and_at_war`), and `ai_soak_takes_a_capital_and_replays` counting stacked plots (none).
- Mutation check: 26 mutants, each condition of the change broken alone. 18 are caught, two of them only by this change's last test lines (another layer's unit; the Builder that passes ours and embarks with 1 MP). One found a redundant condition, now gone (a unit on ours with no moves left cannot wait there either way). Seven change nothing a game can reach: a unit off the map (none stands there; `moveLimits` skips them too), aircraft (they never take move orders), a pass with full moves (it has paid for a step), the wait option a turn later (the pass it adds is beaten by the first), a pass's moves below none (it ends as with none), and in `passOurs` no moves left or the full-moves rule (the search plans no pass that needs them).

## Left open

- Other civs' units. Passing is now in the spec (05: Stacking), which, after the Civ VI wiki, also lets a unit pass the units of a civ it is not at war with. The engine still treats their plots as blocked (`Game::moveLimits`), so a path goes around them.
