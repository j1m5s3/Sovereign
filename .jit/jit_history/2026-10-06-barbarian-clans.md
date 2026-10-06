# Note: the Barbarian Clans mode (step 6, "everything else")

Status: done, 2026-10-06. Chosen by Claude under James's standing consent, after the gap survey found the rules core complete apart from the optional modes. Spec: 01-map-and-terrain (Barbarians, the one-line outline of the optional "Barbarian Clans" mode: camps can be bribed, hired, incited against others, and can become city-states). The values below are Sovereign's own (the spec gives none).

## Built

- `GameSetup::barbarianClans` (save version 68; `sovsim --clans`; Unreal `-SovClans`). Off by default.
- Each camp keeps progress toward becoming a city-state (`Camp::progress`):
  - +2 a turn, and +10 for each bribe or hire;
  - −10 for each of its units killed;
  - at 100 it becomes a city-state (`Game::convertCamp`). It takes an unused city-state type, knows the techs half the major civs know, and keeps the clan's units. This only happens if no city is within 3 plots and an unused type remains.
- Three commands, open to a major civ that has seen the camp (`clanProblem`, `clanCost`):
  - `BribeCamp` (50 gold per era): the camp's units leave that civ alone for 10 turns.
  - `HireFromCamp` (the unit's gold price): the camp's best unit joins the civ beside the camp, at most every 5 turns.
  - `InciteCamp` (100 gold per era): for 10 turns the camp is fully bold, its units leave everyone else alone, and they go up to 15 plots for the target.
  - Durations scale with game speed.
- The AI bribes alerted camps near its cities, hires from them while at war, and incites camps near an enemy's cities, each only with gold to spare (`clans` in `ai.cpp`).
- Unreal: the diplomacy chooser (`N`) lists the bribes, hires and incitements the rules allow.

## Seen

An all-AI game (seed 12, 6 civs, Small, 250 turns, `--clans`): 5 dealings and 4 city-states grown from camps.
